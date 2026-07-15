# Vendor blob cache

Filled by [`scripts/sync_vendor.sh`](../../scripts/sync_vendor.sh).

| Blob | Upstream |
|------|----------|
| `kstuff.elf` | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) |

**Not vendored by OnionHEN anymore:** `elfldr.elf` (9021 service), `ps5debug.elf`, `ps5-app-dumper.elf`, `hen.bin`.

Process spawn uses an **external elfldr service on port 9021**. Built-in daemon / util / kstuff ELFs are sent directly from their embedded memory buffers; user-supplied homebrew ELFs remain file-backed.
