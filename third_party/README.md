# Third-party open-source dependencies

| Submodule | Upstream | Role in OnionHEN |
|-----------|----------|------------------|
| [`kstuff-lite`](kstuff-lite/) | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) | Provides `kstuff.elf` (vendored into bootstrapper) |

## Runtime-only external dependency

[ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) on port 9021 is required at runtime. Its source and binary are not vendored or packaged by OnionHEN.

## Removed from OnionHEN

| Former dependency | Reason |
|-------------------|--------|
| Vendored `elfldr.elf` | The port 9021 service is supplied externally at runtime |
| ps5debug / ps5debug-NG | Optional debugger; not embedded |
| ps5-app-dumper | Optional dump payload; not embedded |
| Byepervisor / hen.bin | 1.xx–2.xx HV path; not bundled |
| Discord RPC | Optional util service; removed |
| libSelfDecryptor | Optional alt decrypt; not used |

```bash
git submodule update --init --recursive
./scripts/sync_vendor.sh
```
