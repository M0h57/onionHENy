# Vendor blob cache

Filled by [`scripts/sync_vendor.sh`](../../scripts/sync_vendor.sh).

| Blob | Upstream |
|------|----------|
| `kstuff.elf` | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) |

**Not used by OrionHEN anymore:** `elfldr.elf` (9021 service), `ps5debug.elf`, `ps5-app-dumper.elf`, `hen.bin`.

Embedded process spawn (daemon / util / kstuff / homebrew ELFs) uses **libelfldr**, kept in sync with the spawn path from [ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) (`third_party/elfldr`).
