# Third-party open-source dependencies (git submodules)

| Submodule | Upstream | Role in OrionHEN |
|-----------|----------|------------------|
| [`elfldr`](elfldr/) | [ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) | Source reference for `source/libelfldr` (embedded spawn). **Not** packaged as a 9021 network service. |
| [`kstuff-lite`](kstuff-lite/) | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) | Provides `kstuff.elf` (vendored into bootstrapper) |

## Removed from OrionHEN

| Former dependency | Reason |
|-------------------|--------|
| `elfldr.elf` / port 9021 service | No longer embedded; spawn is in-process via libelfldr |
| ps5debug / ps5debug-NG | Optional debugger; not embedded |
| ps5-app-dumper | Optional dump payload; not embedded |
| Byepervisor / hen.bin | 1.xx–2.xx HV path; not bundled |
| Discord RPC | Optional util service; removed |
| libSelfDecryptor | Optional alt decrypt; FTP uses local path |

```bash
git submodule update --init --recursive
./scripts/sync_vendor.sh
```
