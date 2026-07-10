# OrionHEN host unit tests

Host-side unit tests for shared libraries and util cheat **file parsing**
(no PS5 SDK). Modeled after `kylin-core/tests`.

## Run

```bash
cd source/util/tests
make test
```

Requirements:

- Host `clang` / `c++` (or set `HOST_CC` / `HOST_CXX`)
- Keystone: `/opt/homebrew` by default (`KEYSTONE_PREFIX=...` if elsewhere)

Optional:

```bash
ORION_TEST_VERBOSE=1 make test   # extra klog-style noise on stderr
make clean && make test
```

Binary: `source/util/build/host-tests/orion-host-tests`

## Coverage

| Suite | What it locks |
|-------|----------------|
| `test_cheat_utils` | hex decode, JSON extract, braces, replace_all, load buffer, ABI layout |
| `test_cheat_parsers` | JSON / SHN / MC4 / ShnExt via factory + real fixtures |
| `test_settings` | full schema serialize/round-trip, partial INI defaults, legacy keys |
| `test_ready` | ready markers, path builder, name rejection, **fps_overlay / util_booted** flags, toolbox legacy alias |
| `test_platform_fs` | `if_exists` / `touch_file` / `rmtree` (liborion_platform) |
| `test_platform_log` | `orion_log_configure` + file sink |
| `test_platform_notify` | `orion_notify_format` prefix/truncate + send stub |
| `test_msg_protocol` | IPC paths, magic, command ordinals, `IPC_Ret`, message POD |

## Intentionally not host-tested

| Area | Why |
|------|-----|
| `CheatApplier` / memory backends | needs target process / mdbg |
| `liborion_proc` allproc / ucred | kernel_copyout |
| ShellUI Mono / OnPress | SceShellUI |
| Full IPC server accept loop | device sockets + daemon world |
| libNineS inject | ptrace |

## Fixtures

`fixtures/cheats/` — small subset of cheat samples (json / shn / mc4 / ShnExt).
