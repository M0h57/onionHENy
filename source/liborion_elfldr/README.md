# liborion_elfldr

Single implementation of ptrace helpers (`pt_*`) and inject-path ELF loading
(`elfldr_load`, `elfldr_payload_args`, `elfldr_raise_privileges`).

## Authid policy

**Do not** flip ucred authid around every `ptrace` syscall.

Callers that need debugger privileges must elevate **once** for the inject
window (see `libNineS` `inject_elf()`), then call into this library. Per-call
authid toggling races multi-threaded injectors and has been observed to
SIGSEGV / hang ShellUI.

Util is elevated once at `main` (`DEBUG_AUTHID`) before cheat code-cave maps.

## Consumers

| Target | Uses |
|--------|------|
| libNineS | `pt_*`, `elfldr_load`, `elfldr_payload_args` |
| bootstrapper | `elfldr_raise_privileges` (spawn via remote 9021) |
| util | `pt_attach` / `pt_mmap` (via `pt_attach_proc` alias) |
| daemon | links via NineS |

Headers: `<orion/pt.h>`, `<orion/elfldr.h>`. Compatibility shims remain under
`libNineS/include/{pt,elfldr}.h` and `bootstrapper/include/`.
