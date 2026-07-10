# Vendor blob cache

Filled by [`scripts/sync_vendor.sh`](../../scripts/sync_vendor.sh).

| Blob | Upstream |
|------|----------|
| `kstuff.elf` | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) |

**Not vendored by OrionHEN anymore:** `elfldr.elf` (9021 service), `ps5debug.elf`, `ps5-app-dumper.elf`, `hen.bin`.

Process spawn (daemon / util / kstuff / homebrew ELFs) uses an **external elfldr service on port 9021**. OrionHEN writes ELFs to disk and asks that service to launch them.
