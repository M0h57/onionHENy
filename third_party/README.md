# Third-party open-source dependencies (git submodules)

| Submodule | Upstream | Role in OnionHEN |
|-----------|----------|------------------|
| [`elfldr`](elfldr/) | [ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) | Optional source reference for the external 9021 loader. **Not** embedded or packaged by OnionHEN. |
| [`kstuff-lite`](kstuff-lite/) | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) | Provides `kstuff.elf` (vendored into bootstrapper) |

## Removed from OnionHEN

| Former dependency | Reason |
|-------------------|--------|
| `elfldr.elf` / port 9021 service | Required externally at runtime; no longer embedded or vendored |
| ps5debug / ps5debug-NG | Optional debugger; not embedded |
| ps5-app-dumper | Optional dump payload; not embedded |
| Byepervisor / hen.bin | 1.xx–2.xx HV path; not bundled |
| Discord RPC | Optional util service; removed |
| libSelfDecryptor | Optional alt decrypt; not used |

```bash
git submodule update --init --recursive
./scripts/sync_vendor.sh
```
