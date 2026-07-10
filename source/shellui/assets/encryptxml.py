#!/usr/bin/env python3
"""XOR-encrypt toolbox XML for embedding as OrionHEN_toolbox.sxml.

Must use the *decoded* key bytes so runtime decrypt (base64_decode then XOR)
matches. Historical bug: this script XOR'd with the Base64 string itself while
C++ decrypted with base64_decode(key) → garbage XML and broken menus.
"""

from __future__ import annotations

import base64
from pathlib import Path

# Same constant as shellui kXorKeyB64; decoded value is the real XOR key.
KEY_B64 = b"U0lTVFIwX0lfU0VFX1lPVQ=="


def xor_bytes(data: bytes, key: bytes) -> bytes:
    if not key:
        raise ValueError("empty XOR key")
    kl = len(key)
    return bytes(b ^ key[i % kl] for i, b in enumerate(data))


def main() -> None:
    # CMake runs this with WORKING_DIRECTORY = source/
    root = Path(__file__).resolve().parent
    src = root / "OrionHEN_toolbox.xml"
    dst = root / "OrionHEN_toolbox.sxml"
    key = base64.b64decode(KEY_B64)
    data = src.read_bytes()
    enc = xor_bytes(data, key)
    dst.write_bytes(enc)
    # Sanity: decrypt round-trip
    dec = xor_bytes(enc, key)
    if dec != data:
        raise SystemExit("encryptxml: round-trip mismatch")
    if b"system_settings" not in dec:
        raise SystemExit("encryptxml: missing system_settings after round-trip")
    print(f"encryptxml: wrote {dst} ({len(enc)} bytes, key_len={len(key)})")


if __name__ == "__main__":
    main()
