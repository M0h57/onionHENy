# ShellUI injection flow & kylin-core libNineS fixes

## Call path (OnionHEN)

```
daemon: cmd_enable_toolbox()          [daemon/source/msg.cpp]
  ├─ get_shellui_pid()                // find SceShellUI
  └─ Inject_Toolbox(pid, shellui.elf) [libNineS/src/main.c]
       └─ inject_elf(proc, elf)       [libNineS/src/injector.c]
            ├─ set_ucred_to_ptrace() once (PTRACE_AUTHID)
            ├─ pt_attach(SceShellUI)
            ├─ init_remote_function_pointers()  // malloc, pthread_create, …
            ├─ elfldr_load()                    // map ELF into target
            ├─ elfldr_payload_args()
            ├─ mmap bootstrap stager + copyin
            ├─ mmap SCEFunctions blob + copyin
            ├─ pt_call2(bootstrap) → remote pthread_create(elf_main)
            │     stager ends with int3
            └─ pt_detach + restore authid
```

After all required hooks are installed, the ShellUI payload publishes its own
PID in `/system_tmp/onion_ready/toolbox` (and the legacy
`/system_tmp/toolbox_online` alias). The daemon waits up to 45 seconds for the
expected PID, not merely for file existence.

The marker is intentionally retained. Before injecting, the daemon compares
its value with the current `SceShellUI` PID:

- same PID: that ShellUI process instance is already initialized; skip inject;
- different/missing PID: clear stale state, inject, then wait for the new PID;
- ShellUI changes PID during the handshake: reject the acknowledgement.

`ToolboxInjectionCoordinator` serializes this check/inject/wait sequence so
cold-start and util reinjection requests cannot ptrace the same ShellUI in
parallel. This follows kstuff-lite's process-instance model while retaining the
stronger post-hook ready acknowledgement.

kylin-core uses the same `inject_elf()` path for ShellUI overlay injection (`overlay_service.c` → `inject_elf`), plus higher-level retries / kill-respawn / boot-id skip that live outside libNineS.

## Inconsistencies found (OnionHEN vs kylin-core)

| Area | OnionHEN (before) | kylin-core | Risk |
|------|-----------------|------------|------|
| `sys_ptrace` | Flip authid to debugger **on every** ptrace call, then restore | Direct `syscall(SYS_ptrace)` | **Thread-unsafe race** between concurrent ptrace ops; can SIGSEGV daemon or corrupt authid |
| `pt_call2` | `pt_continue` then **immediately** `pt_setregs(bak)` | `pt_continue` → **`waitpid`** → then restore regs | **Critical race**: restores ShellUI registers while bootstrap still runs → crash / freeze / power loss |
| `inject_elf` authid | wrong id / no restore (historical) | `set_ucred_to_ptrace()` + restore on all exits | Wrong id → attach fails; no restore → daemon stuck elevated |
| Error paths | Often leave `status=true` on load/mmap failure | Set `status=false` on each failure | Caller thinks inject succeeded |
| `MAP_FAILED` | Only checks `!bootstrap` | Also rejects `(uint64_t)-1` | mmap failure treated as success addr |
| mprotect / copyin | Unchecked | Checked, fail inject | Partial maps then trigger entry |
| `attached` flag | Not cleared after detach | Cleared when detach succeeds | Stale attach state on retry |

## Fixes adopted in OnionHEN

Ported into `source/libNineS/src/pt.c` and `source/libNineS/src/injector.c` (without kylin-specific logger; uses `ps5/klog`).

1. **`set_ucred_to_ptrace()`** at `inject_elf` entry/exit — sets  
   `PTRACE_AUTHID` (`0x4800000000010003`), never `DEBUG_AUTHID` (`…0006`).  
2. **No per-ptrace authid flip** in `sys_ptrace`  
3. **`waitpid` after stager `pt_continue`** in `pt_call2`  
4. **Strict validation** of null args, entry/args, mmap, mprotect, copyin  
5. **Clear `attached`** on successful detach; always restore prior authid  

## Not ported (higher-level kylin-core only)

These live in `kylin-core/src/services/overlay_service.c`, not libNineS:

- Multi-attempt inject + kill ShellUI + wait for respawn

OnionHEN now provides in-process injection serialization and PID-bound
already-injected detection. Optional follow-up: add retry/respawn around
`cmd_enable_toolbox()` if field failure rate stays high.

## Upstream files for reference

- `kylin-core/third_party/libNineS/src/pt.c`
- `kylin-core/third_party/libNineS/src/injector.c`
- `kylin-core/src/services/overlay_service.c`
