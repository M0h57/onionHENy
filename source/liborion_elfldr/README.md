# liborion_elfldr

Single implementation of ptrace helpers (`pt_*`) and inject-path ELF loading
(`elfldr_load`, `elfldr_payload_args`, `elfldr_raise_privileges`).

## Authid policy

**Do not** flip ucred authid around every `ptrace` syscall.

Callers that need ptrace must elevate **once** for the inject window with
**`PTRACE_AUTHID` (`0x4800000000010003`)** via `set_ucred_to_debugger()` —
see `libNineS` `inject_elf()`. That is Sony's SceTracer-style id;  
`DEBUG_AUTHID` (`0x4800000000000006`) is **not** accepted by the kernel
ptrace check (attach then fails with `waitpid`/`ECHILD`).

Per-call authid toggling races multi-threaded injectors and has been observed
to SIGSEGV / hang ShellUI.

Util should elevate with the same ptrace authid before `pt_attach` / cave maps.

## Consumers

| Target | Uses |
|--------|------|
| libNineS | `pt_*`, `elfldr_load`, `elfldr_payload_args` |
| bootstrapper | `elfldr_raise_privileges` (spawn via remote 9021) |
| util | `pt_attach` / `pt_mmap` (via `pt_attach_proc` alias) |
| daemon | links via NineS |

Headers: `<orion/pt.h>`, `<orion/elfldr.h>`. Compatibility shims remain under
`libNineS/include/{pt,elfldr}.h` and `bootstrapper/include/`.
