# util cheat host tests

Host-side unit tests for cheat **file parsing** (no PS5 SDK), modeled after
`kylin-core/tests` (`test_cheat_engine` / `test_cheat_utils`).

## What is covered

| Suite | Coverage |
|-------|----------|
| `test_cheat_utils` | hex decode, JSON extract, brace matching, replace_all, load buffer, ABI layout |
| `test_cheat_parsers` | JSON / SHN / MC4 / ShnExt via `CheatParserFactory`, plus real fixtures |
| `test_settings` | `orion::Settings` serialize keys, file round-trip, legacy key, defaults |
| `test_ready` | ready marker signal / wait / clear / path rejection |

Runtime apply (`CheatApplier` / memory backends) is **not** host-tested.

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
ORION_TEST_VERBOSE=1 make test   # enable OrionHEN_log on stderr
```

## Fixtures

`fixtures/cheats/` is a small subset of kylin-core cheat samples used by the
parser suite (json / shn / mc4 / ShnExt).
