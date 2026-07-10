# OrionHEN Cheats (C++ orchestration)

## Architecture

See [docs/util_arch/cheats_cpp.md](../../../../docs/util_arch/cheats_cpp.md).

```text
IPC / main
  └─ orion::cheats::CheatService          # Facade + mutex + hot-reload state
       ├─ CheatRepository                 # path resolve, load, flatten
       │    └─ CheatParserFactory         # Strategy by extension
       │         ├─ JsonCheatParser
       │         ├─ XmlCheatParser (.shn)
       │         ├─ Mc4CheatParser
       │         └─ ShnExtCheatParser → C
       ├─ CheatApplier                    # apply patches
       │    └─ IMemoryBackend (Strategy)
       │         ├─ MdbgMemoryBackend
       │         └─ KdirectMemoryBackend  # MemoryBackendFactory
       └─ C helpers                       # utils / ShnExt crypto / flatten
```

## Patterns

| Pattern | Class |
|---------|--------|
| Facade | `CheatService` |
| Singleton | `CheatService::instance()` |
| Strategy | `IMemoryBackend`, `ICheatParser` |
| Factory | `MemoryBackendFactory`, `CheatParserFactory` |
| Adapter | `ShnExtCheatParser` |
| RAII | `std::lock_guard` on service state |

## Formats / paths

```text
/data/OrionHEN/cheats/<TITLE_ID>_<VERSION>.{json,shn,mc4,ShnExt}
```

Priority: json → shn → mc4 → ShnExt. No KCF/WMDW.

## Memory

| FW major | Backend |
|---------|---------|
| `< 0x840` | mdbg |
| `>= 0x840` | kdirect |

Code cave: `pt_attach_proc` + `pt_mmap` + `kernel_mprotect`.
