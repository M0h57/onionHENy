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
| `test_cheat_flatten` | extension match + GoldHEN-style flat install names + version sanitize |
| `test_plugin` | `liborion_plugin` ELF magic, package header, pid path/file, read_file |
| `test_base64` | encode / decode / round-trip (MC4 codec) |
| `test_aes_cbc` | AES-256-CBC encrypt/decrypt with MC4 key/IV |
| `test_hde64` | x86_64 length decode (nop/ret/mov/jmp) |
| `test_http_github` | GitHub commits JSON → `sha` (object + array) |
| `test_reg_entity` | registry entity-id formula (account slots) |
| `test_account_id_b64` | uint64 account id → base64 |
| `test_playtime` | binary playtime store (TID10 + u64) |
| `test_toolbox_helpers` | UI path rewrite + plugin/elf basename filter |
| `test_settings` | full schema serialize/round-trip, partial INI defaults, legacy keys |
| `test_ready` | ready markers, path builder, name rejection, **fps_overlay / util_booted** flags, toolbox legacy alias |
| `test_platform_fs` | `if_exists` / `touch_file` / `rmtree` (liborion_platform) |
| `test_platform_log` | `orion_log_configure` + file sink |
| `test_platform_notify` | `orion_notify_format` prefix/truncate + send stub |
| `test_msg_protocol` | IPC paths, magic, command ordinals, `IPC_Ret`, message POD, reply JSON body |
| `test_ps5_settings_ui` | fluent XML builder + escaping |
| `test_toolbox_route` | resource → page routing + cheat map helpers |

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
