# OrionHEN — source tree

This directory is the CMake project for **OrionHEN**, a community continuation of **etaHEN** (LightningMods), licensed under **GPLv3**.

OrionHEN is based on etaHEN’s open-source release. See the [root README](../README.md) for project goals, credits to **GoldHEN** and **etaHEN**, and full documentation.

## Layout

| Path | Description |
|------|-------------|
| `bootstrapper/` | Main payload bootstrapper + Byepervisor |
| `daemon/` | Main daemon |
| `util/` | Utility daemon (FTP, PKG, …) |
| `shellui/` | Toolbox / ShellUI hooks |
| `fps_elf/` | FPS overlay PRX |
| `unpacker/` | Payload unpacker |
| `libhijacker/`, `libelfldr/`, `libNineS/`, `libNidResolver/`, `libSelfDecryptor/` | Internal static libs |
| `extern/` | Third-party sources (cJSON, pugixml, tiny-json, 7zip-sdk, …) |
| `include/` | Shared headers |
| `lib/` | Prebuilt link libraries (static + PS5 stubs) + `backtrace.cpp` |
| `stubber/` | NID stub generation (Go) |

## Build

Requires a Prospero / PS5 payload SDK (`PS5_PAYLOAD_SDK`) and clang targeting `x86_64-sie-ps5`.

From the repo root:

```bash
./scripts/ps5_cmake.sh -S source -B source/build
cmake --build source/build
```

Or use `CMakePresets.json` in this directory. Build products go under `source/bin/` (gitignored).

## Further reading

- Project overview & credits: [`../README.md`](../README.md)
- Writeups: [`../docs/`](../docs/)
- Upstream lineage: [etaHEN](https://github.com/LightningMods/etaHEN) · [GoldHEN](https://github.com/GoldHEN/GoldHEN)
