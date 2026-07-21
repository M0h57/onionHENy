#!/usr/bin/env python3
"""Two-pass offline verifier: HomeUI top-nav Fix A (no crash) + Fix B (focus icon).

Reads original NPXS40002 dumps, matches C++ HomeUiPatchProfiles, simulates the
runtime Hermes patches, and checks product invariants twice for consistency.

Fix A — game-close crash:
  * Fps body remains stock showFps prefix (not OnionHEN)
  * ApplicationErrorEventTrigger is the OnionHEN button host (full 77-byte body)
  * top-nav order is [Search, ApplicationErrorEventTrigger, Settings, Profile]

Fix B — focused icon (HBC side):
  * custom icon string is /system_ex/vsh_asset/onionhen.png
  * runtime SetIconSource→invertedIcon mirror keys off this path
"""

from __future__ import annotations

import hashlib
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
CPP = (REPO / "source/shellui/src/homeui_top_nav_patch.cpp").read_text()
HERMES = bytes([0xC6, 0x1F, 0xBC, 0x03, 0xC1, 0x03, 0x19, 0x1F])

# Exact dump list requested by user (paths resolved under sibling tree).
DUMP_ROOT = Path("/Users/chenpy/Projects/Person/ps5-kylin")
DUMPS = [
    DUMP_ROOT / "12.7DUIMP",
    DUMP_ROOT / "12.6DUMP",
    DUMP_ROOT / "12.4DUMP",
    DUMP_ROOT / "12.02DUMP",
    DUMP_ROOT / "12.2DUMP",
    DUMP_ROOT / "12.0DUMP",
    DUMP_ROOT / "11.6DUMP",
    DUMP_ROOT / "11.4DUMP",
    DUMP_ROOT / "11.2DUMP",
    DUMP_ROOT / "11.0DUMP",
    DUMP_ROOT / "10.6DUMP",
    DUMP_ROOT / "10.4DUMP",
    DUMP_ROOT / "10.2DUMP",
    DUMP_ROOT / "10.01DUMP",
]

# Mirrors source/shellui/src/homeui_top_nav_patch.cpp kHomeUiPatchProfiles.
# offset keys match HomeUiPatchOffsets order including app_error_body.
PROFILES = [
    {
        "name": "10.01 NPXS40002 HomeUI",
        "byte_set": "1001",
        "file_length": 0x1B3318,
        "source_hash": bytes.fromhex(
            "ae256655beaeea9e752e23256bbdddf1af254aea"
        ),
        "offsets": {
            "title_id": 0x4768E,
            "app_error_event_trigger": 0x4444C,
            "navigate_to_home": 0x53B35,
            "home_icon_order": 0xBCC22,
            "fps_factory": 0x1752C3,
            "download_error_string": 0x39D18,
            "custom_icon_value": 0xC0129,
            "custom_icon_uri": 0x520C5,
            "top_nav_link_uri": 0x49CC6,
            "custom_title_value": 0xC012D,
            "fps_body": 0x175A00,
            "app_error_body": 0x175967,
        },
    },
    {
        "name": "10.4/10.6 NPXS40002 HomeUI",
        "byte_set": "1060",
        "file_length": 0x1B3BCC,
        "source_hash": bytes.fromhex(
            "2cac5cc444ba0473ea8ee632a7942f281482a68a"
        ),
        "offsets": {
            "title_id": 0x473FA,
            "app_error_event_trigger": 0x44718,
            "navigate_to_home": 0x53990,
            "home_icon_order": 0xBCD9C,
            "fps_factory": 0x17593C,
            "download_error_string": 0x39D0C,
            "custom_icon_value": 0xC0259,
            "custom_icon_uri": 0x4CD5E,
            "top_nav_link_uri": 0x499ED,
            "custom_title_value": 0xC025D,
            "fps_body": 0x176079,
            "app_error_body": 0x175FE0,
        },
    },
    {
        "name": "11.0 NPXS40002 HomeUI",
        "byte_set": "1100",
        "file_length": 0x1B3010,
        "source_hash": bytes.fromhex(
            "e21110895e8fb6c85f49db972d51fc101bb8fc52"
        ),
        "offsets": {
            "title_id": 0x451C1,
            "app_error_event_trigger": 0x44542,
            "navigate_to_home": 0x53E2C,
            "home_icon_order": 0xBD018,
            "fps_factory": 0x1765A2,
            "download_error_string": 0x3A14D,
            "custom_icon_value": 0xC0532,
            "custom_icon_uri": 0x5368C,
            "top_nav_link_uri": 0x49D54,
            "custom_title_value": 0xC0536,
            "fps_body": 0x176998,
            "app_error_body": 0x1768FF,
        },
    },
    {
        "name": "11.4/11.6 NPXS40002 HomeUI",
        "byte_set": "1160",
        "file_length": 0x1B2CC8,
        "source_hash": bytes.fromhex(
            "f321f83f9143035f5d97ee5ad98ceb75133c890e"
        ),
        "offsets": {
            "title_id": 0x450F2,
            "app_error_event_trigger": 0x443FB,
            "navigate_to_home": 0x53B3E,
            "home_icon_order": 0xBCF40,
            "fps_factory": 0x175CBD,
            "download_error_string": 0x3A01F,
            "custom_icon_value": 0xC0451,
            "custom_icon_uri": 0x533ED,
            "top_nav_link_uri": 0x49CD3,
            "custom_title_value": 0xC0455,
            "fps_body": 0x1760B3,
            "app_error_body": 0x17601A,
        },
    },
    {
        "name": "12.7 NPXS40002 HomeUI",
        "byte_set": "1270",
        "file_length": 0x1B73BC,
        "source_hash": bytes.fromhex(
            "9dd2dc47c6024843f685af80ae9273e6a075337d"
        ),
        "offsets": {
            "title_id": 0x46444,
            "app_error_event_trigger": 0x455BA,
            "navigate_to_home": 0x55558,
            "home_icon_order": 0xBF71A,
            "fps_factory": 0x1793B7,
            "download_error_string": 0x468EE,
            "custom_icon_value": 0xC3049,
            "custom_icon_uri": 0x54E9F,
            "top_nav_link_uri": 0x4B726,
            "custom_title_value": 0xC304D,
            "fps_body": 0x1797AD,
            "app_error_body": 0x179714,
        },
    },
    {
        "name": "12.20 NPXS40002 HomeUI",
        "byte_set": "1220",
        "file_length": 0x1B70E4,
        "source_hash": bytes.fromhex(
            "d9aa3ec2fcf7cc0bb0a7fe6362079c494948cf5e"
        ),
        "offsets": {
            "title_id": 0x46424,
            "app_error_event_trigger": 0x4559A,
            "navigate_to_home": 0x55598,
            "home_icon_order": 0xBF6CA,
            "fps_factory": 0x1792B3,
            "download_error_string": 0x468CE,
            "custom_icon_value": 0xC2FFA,
            "custom_icon_uri": 0x54EDF,
            "top_nav_link_uri": 0x4B7CF,
            "custom_title_value": 0xC2FFE,
            "fps_body": 0x1796A9,
            "app_error_body": 0x179610,
        },
    },
]


def extract_array(name: str) -> bytes:
    m = re.search(
        rf"static const unsigned char {re.escape(name)}\[\] = \{{([^}}]+)\}};",
        CPP,
        re.S,
    )
    if not m:
        raise SystemExit(f"missing C++ array {name}")
    body = m.group(1)
    hx = re.findall(r"0x([0-9a-fA-F]{2})", body)
    if hx:
        return bytes(int(x, 16) for x in hx)
    chars = re.findall(r"'((?:\\.|[^'\\]))'", body)
    out = bytearray()
    escapes = {"n": 10, "r": 13, "t": 9, "0": 0, "\\": 92, "'": 39}
    for c in chars:
        if c.startswith("\\") and len(c) == 2:
            out.append(escapes.get(c[1], ord(c[1])))
        else:
            out.append(ord(c))
    return bytes(out)


def load_bytesets() -> dict:
    out = {}
    for bs in ("1001", "1060", "1100", "1160", "1270", "1220"):
        out[bs] = {
            "old_order": extract_array(f"k{bs}OldIconOrder"),
            "app_error_order": extract_array(f"k{bs}LegacyAppErrorIconOrder"),
            "fps_slot_order": extract_array(f"k{bs}NewIconOrder"),
            "stock_app_error": extract_array(f"k{bs}StockAppErrorBody"),
            "onion": extract_array(f"k{bs}OnionHenButtonBody"),
            "old_fps_prefix": extract_array(f"k{bs}OldFpsBodyPrefix"),
            "old_icon_val": extract_array(f"k{bs}OldCustomIconValue"),
            "new_icon_val": extract_array(f"k{bs}NewCustomIconValue"),
            "old_title_val": extract_array(f"k{bs}OldCustomTitleValue"),
        }
        assert (
            len(out[bs]["stock_app_error"])
            == len(out[bs]["onion"])
            == len(out[bs]["old_fps_prefix"])
            == 77
        ), bs
    return out


BYTESETS = load_bytesets()
LEGACY_1160_ONION = extract_array("k1160LegacyOnionHenButtonBody")
NEW_ICON_URI = extract_array("kNewCustomIconUri")
OLD_ICON_URI = extract_array("kOldCustomIconUri")
NEW_LINK = extract_array("kNewTopNavLinkUri")
OLD_LINK = extract_array("kOldTopNavLinkUri")
NEW_TITLE = extract_array("kNewCustomTitleValue")
STOCK_DL = extract_array("kStockDownloadErrorString")
BLANK_LINK = extract_array("kLegacyBlankTopNavLinkUri")
PADDED_LINK = extract_array("kLegacyPaddedTopNavLinkUri")

assert NEW_ICON_URI == b"/system_ex/vsh_asset/onionhen.png", NEW_ICON_URI
assert NEW_LINK == b"OnionHEN?NavUI=1", NEW_LINK


def locate_hbc(data: bytes) -> bytearray | None:
    if data.startswith(HERMES):
        return bytearray(data)
    if data[:8] == b"RNPSHEDR":
        off = struct.unpack_from("<I", data, 0x1C)[0]
        if off == 0 or off >= len(data):
            off = 0xB20
        if data[off : off + 8] == HERMES:
            return bytearray(data[off:])
    i = data.find(HERMES)
    return bytearray(data[i:]) if i >= 0 else None


def match_profile(hbc: bytes) -> dict | None:
    fl = struct.unpack_from("<I", hbc, 0x20)[0]
    sh = bytes(hbc[0x0C:0x20])
    ver = struct.unpack_from("<I", hbc, 0x08)[0]
    for p in PROFILES:
        if not (p["file_length"] == fl and p["source_hash"] == sh and ver == 89):
            continue
        o = p["offsets"]
        if (
            bytes(hbc[o["title_id"] : o["title_id"] + 9]) == b"NPXS40002"
            and bytes(
                hbc[
                    o["app_error_event_trigger"] : o["app_error_event_trigger"]
                    + 28
                ]
            )
            == b"ApplicationErrorEventTrigger"
            and bytes(hbc[o["navigate_to_home"] : o["navigate_to_home"] + 23])
            == b"pshomeui:navigateToHome"
        ):
            return p
    return None


def patch_at(hbc, name, off, expected_opts, replacement, notes) -> bool:
    cur = bytes(hbc[off : off + len(replacement)])
    if cur == replacement:
        notes.append(f"{name}:already")
        return True
    for e in expected_opts:
        if e is not None and cur == e:
            hbc[off : off + len(replacement)] = replacement
            notes.append(f"{name}:applied")
            return True
    notes.append(f"{name}:MISMATCH head={cur[:12].hex()}")
    return False


def apply_patch(hbc: bytearray, p: dict) -> list[str]:
    bs = BYTESETS[p["byte_set"]]
    o = p["offsets"]
    notes: list[str] = []
    ok = True
    ok &= patch_at(
        hbc,
        "icon_order",
        o["home_icon_order"],
        [bs["old_order"], bs["fps_slot_order"], bs["app_error_order"]],
        bs["app_error_order"],
        notes,
    )
    fps_alts = [bs["onion"], bs["old_fps_prefix"]]
    if p["byte_set"] == "1160":
        fps_alts.append(LEGACY_1160_ONION)
    ok &= patch_at(
        hbc,
        "fps_repair",
        o["fps_body"],
        fps_alts,
        bs["old_fps_prefix"],
        notes,
    )
    app_alts = [bs["stock_app_error"], bs["onion"]]
    if p["byte_set"] == "1160":
        app_alts.append(LEGACY_1160_ONION)
    ok &= patch_at(
        hbc,
        "app_error_onion",
        o["app_error_body"],
        app_alts,
        bs["onion"],
        notes,
    )
    ok &= patch_at(
        hbc,
        "icon_uri",
        o["custom_icon_uri"],
        [OLD_ICON_URI, NEW_ICON_URI],
        NEW_ICON_URI,
        notes,
    )
    ok &= patch_at(
        hbc,
        "link_uri",
        o["top_nav_link_uri"],
        [OLD_LINK, NEW_LINK, BLANK_LINK, PADDED_LINK],
        NEW_LINK,
        notes,
    )
    ok &= patch_at(
        hbc,
        "icon_val",
        o["custom_icon_value"],
        [bs["old_icon_val"], bs["new_icon_val"]],
        bs["new_icon_val"],
        notes,
    )
    ok &= patch_at(
        hbc,
        "title_val",
        o["custom_title_value"],
        [bs["old_title_val"], NEW_TITLE],
        NEW_TITLE,
        notes,
    )
    ok &= patch_at(
        hbc,
        "download_error",
        o["download_error_string"],
        [STOCK_DL, b"OnionHEN?NavUI"],
        STOCK_DL,
        notes,
    )
    fl = p["file_length"]
    footer = fl - 20
    hbc[footer : footer + 20] = hashlib.sha1(bytes(hbc[:footer])).digest()
    if not ok:
        notes.append("APPLY_FAILED")
    return notes


def verify(hbc: bytes, p: dict, tag: str) -> list[str]:
    errs: list[str] = []
    bs = BYTESETS[p["byte_set"]]
    o = p["offsets"]

    order = bytes(hbc[o["home_icon_order"] : o["home_icon_order"] + 9])
    if order != bs["app_error_order"]:
        errs.append(f"{tag}: order not AppError-slot ({order.hex()})")

    fps = bytes(hbc[o["fps_body"] : o["fps_body"] + 77])
    app = bytes(hbc[o["app_error_body"] : o["app_error_body"] + 77])

    # Fix A — crash host must not be Fps
    if fps == bs["onion"] or (
        p["byte_set"] == "1160" and fps == LEGACY_1160_ONION
    ):
        errs.append(f"{tag}: FixA FAIL — Fps still OnionHEN (crash regression)")
    if fps != bs["old_fps_prefix"]:
        errs.append(f"{tag}: FixA FAIL — Fps prefix not stock ({fps[:8].hex()})")
    if app != bs["onion"]:
        errs.append(
            f"{tag}: FixA FAIL — AppError not OnionHEN host ({app[:8].hex()})"
        )
    if app == bs["stock_app_error"]:
        errs.append(f"{tag}: FixA FAIL — AppError still stock")
    if o["fps_body"] - o["app_error_body"] != 153:
        errs.append(
            f"{tag}: AppError→Fps gap {o['fps_body'] - o['app_error_body']} != 153"
        )

    # Fix B — focus icon path present for invertedIcon mirror
    uri = bytes(hbc[o["custom_icon_uri"] : o["custom_icon_uri"] + 33])
    if uri != NEW_ICON_URI:
        errs.append(f"{tag}: FixB FAIL — icon uri not onionhen.png ({uri!r})")
    if NEW_ICON_URI not in bytes(hbc):
        errs.append(f"{tag}: FixB FAIL — onionhen.png absent from HBC")

    link = bytes(hbc[o["top_nav_link_uri"] : o["top_nav_link_uri"] + 16])
    if link != NEW_LINK:
        errs.append(f"{tag}: link not OnionHEN?NavUI=1 ({link!r})")

    de = bytes(hbc[o["download_error_string"] : o["download_error_string"] + 14])
    if de != STOCK_DL:
        errs.append(f"{tag}: download_error string broken ({de!r})")

    if bytes(hbc[o["title_id"] : o["title_id"] + 9]) != b"NPXS40002":
        errs.append(f"{tag}: NPXS40002 marker broken")

    fl = p["file_length"]
    footer = fl - 20
    if hashlib.sha1(bytes(hbc[:footer])).digest() != bytes(
        hbc[footer : footer + 20]
    ):
        errs.append(f"{tag}: footer SHA1 invalid")

    if app[:2] == bytes.fromhex("3204"):
        errs.append(f"{tag}: AppError still CreateEnvironment stock head")
    return errs


def precheck(hbc: bytes, p: dict) -> list[str]:
    errs: list[str] = []
    bs = BYTESETS[p["byte_set"]]
    o = p["offsets"]
    order = bytes(hbc[o["home_icon_order"] : o["home_icon_order"] + 9])
    if order not in (bs["old_order"], bs["fps_slot_order"], bs["app_error_order"]):
        errs.append(f"pre: unexpected order {order.hex()}")
    app = bytes(hbc[o["app_error_body"] : o["app_error_body"] + 77])
    if app not in (bs["stock_app_error"], bs["onion"]) and not (
        p["byte_set"] == "1160" and app == LEGACY_1160_ONION
    ):
        errs.append(f"pre: AppError body unexpected ({app[:8].hex()})")
    # Virgin dumps must match C++ stock AppError exactly.
    if app != bs["stock_app_error"] and app != bs["onion"]:
        errs.append("pre: AppError neither stock nor already-onion")
    if app == bs["stock_app_error"] and app != bs["stock_app_error"]:
        pass
    if bytes(hbc[o["app_error_body"] : o["app_error_body"] + 77]) != bs[
        "stock_app_error"
    ]:
        # If dump is already patched onion, still OK for migration path.
        if bytes(hbc[o["app_error_body"] : o["app_error_body"] + 77]) != bs["onion"]:
            errs.append("pre: AppError body != C++ stock (and not onion)")
    else:
        # Exact match stock — good.
        pass
    fps = bytes(hbc[o["fps_body"] : o["fps_body"] + 77])
    if fps not in (bs["old_fps_prefix"], bs["onion"]) and not (
        p["byte_set"] == "1160" and fps == LEGACY_1160_ONION
    ):
        errs.append(f"pre: Fps prefix unexpected ({fps[:8].hex()})")
    # Require virgin AppError/Fps/order match C++ for true original dumps.
    if (
        order == bs["old_order"]
        and app != bs["stock_app_error"]
    ):
        errs.append("pre: virgin order but AppError not stock")
    if app == bs["stock_app_error"] and app != bytes(
        hbc[o["app_error_body"] : o["app_error_body"] + 77]
    ):
        errs.append("pre: stock AppError compare bug")
    # Strong check: stock body bytes equal C++ table
    if order == bs["old_order"]:
        if app != bs["stock_app_error"]:
            errs.append("pre: stock dump AppError != C++ StockAppErrorBody")
        if fps != bs["old_fps_prefix"]:
            errs.append("pre: stock dump Fps prefix != C++ OldFpsBodyPrefix")
    return errs


def run_pass(pass_id: int):
    print(f"\n========== PASS {pass_id} ==========")
    rows = []
    for d in DUMPS:
        name = d.name
        if not d.is_dir():
            print(f"[MISS] {name}")
            rows.append((name, False, ["missing dir"]))
            continue
        path = d / "NPXS40002.bin"
        if not path.exists():
            print(f"[MISS] {name}: no NPXS40002.bin")
            rows.append((name, False, ["no NPXS40002.bin"]))
            continue
        hbc = locate_hbc(path.read_bytes())
        if hbc is None:
            print(f"[FAIL] {name}: no HBC")
            rows.append((name, False, ["no HBC"]))
            continue
        fl = struct.unpack_from("<I", hbc, 0x20)[0]
        if fl <= 0 or fl > len(hbc):
            print(f"[FAIL] {name}: bad file_length 0x{fl:x}")
            rows.append((name, False, [f"bad fl 0x{fl:x}"]))
            continue
        hbc = hbc[:fl]
        p = match_profile(hbc)
        if not p:
            sh = bytes(hbc[0x0C:0x20]).hex()
            print(f"[FAIL] {name}: no profile fl=0x{fl:x} hash={sh}")
            rows.append((name, False, ["no profile"]))
            continue

        errs: list[str] = []
        errs += precheck(hbc, p)
        notes1 = apply_patch(hbc, p)
        errs += verify(hbc, p, "after-patch")

        hbc2 = bytearray(hbc)
        errs += verify(hbc2, p, "reread")
        notes2 = apply_patch(hbc2, p)
        errs += verify(hbc2, p, "idempotent")
        if any("MISMATCH" in x or "APPLY_FAILED" in x for x in notes1 + notes2):
            errs.append(f"apply:{notes1}|{notes2}")

        ok = not errs
        print(
            f"[{'OK' if ok else 'FAIL'}] {name:12} → {p['name']} "
            f"[bs={p['byte_set']}]"
        )
        if ok:
            print(
                "       FixA OK: Fps=stock; AppError=OnionHEN; "
                "order=Search|AppError|Settings|Profile"
            )
            print(
                f"       FixB OK: icon={NEW_ICON_URI.decode()} "
                f"link={NEW_LINK.decode()}"
            )
        else:
            for e in errs:
                print(f"       {e}")
        rows.append((name, ok, errs))
    return rows


def main() -> int:
    print(f"Repo: {REPO}")
    print(f"Profiles: {len(PROFILES)}  Dumps: {len(DUMPS)}")
    print(f"NEW_ICON_URI={NEW_ICON_URI!r}")
    print(f"NEW_LINK={NEW_LINK!r}")

    r1 = run_pass(1)
    r2 = run_pass(2)

    def summarize(rows, tag: str) -> bool:
        ok = sum(1 for _, o, _ in rows if o)
        print(f"\n{tag}: {ok}/{len(rows)} OK")
        for n, o, e in rows:
            if not o:
                print(f"  FAIL {n}: {e}")
        return ok == len(rows)

    s1 = summarize(r1, "PASS1")
    s2 = summarize(r2, "PASS2")
    consistent = all(a[0] == b[0] and a[1] == b[1] for a, b in zip(r1, r2))
    print(f"\nCONSISTENT: {'YES' if consistent else 'NO'}")
    overall = s1 and s2 and consistent
    print(f"OVERALL: {'PASS' if overall else 'FAIL'}")

    print("\n=== Dump → profile ===")
    for n, ok, _ in r1:
        print(f"  {n:12} {'OK' if ok else 'FAIL'}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
