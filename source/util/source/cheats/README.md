# OrionHEN Cheat Engine

Flat path + mdbg/kdirect memory writes. Formats: **json / shn / mc4 / ShnExt** (no KCF/WMDW).

## Layout

```text
/data/OrionHEN/cheats/<TITLE_ID>_<VERSION>.{json,shn,mc4,ShnExt}
```

## Platform reuse

Cheats do **not** carry a private process/fw/file stack. Shared helpers live in:

| API | Purpose |
|-----|---------|
| `util_system_fw_major()` | Prospero SW major (mdbg vs kdirect). *Not* `kern.sdk_version` (hijacker offsets). |
| `util_file_read_alloc()` | Whole-file read for loaders / version JSON+SFO |
| `util_resolve_game_version()` | Same path discovery idea as `BREW_UTIL_GET_GAME_VER` |
| `util_get_running_bigapp()` | Current BigApp pid/title/process |
| `util_find_module()` | Dynlib lookup (same rules as NineS `get_module_info`) + eboot base fallback |

NineS `get_module_info()` / improved `get_module_handle()` name matching is shared by injector and other callers.

## Memory backends

| FW major | Backend |
|---------|---------|
| `< 0x840` | mdbg |
| `>= 0x840` | kdirect |

Code-cave uses existing `pt_attach_proc` / `pt_mmap` / `pt_detach_proc` (no `pt_*_proc` shim layer).
