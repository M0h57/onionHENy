#!/usr/bin/env python3
"""Generate the compile-time toolbox translation table from locale JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_catalog(path: Path) -> dict[str, str]:
    try:
        data: Any = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"error: cannot load {path}: {error}") from error

    if not isinstance(data, dict) or not data:
        raise SystemExit(f"error: {path} must contain a non-empty JSON object")

    invalid = [key for key, value in data.items()
               if not isinstance(key, str) or not key
               or not isinstance(value, str) or "\0" in value]
    if invalid:
        raise SystemExit(
            f"error: {path} contains invalid keys or non-string values: {invalid}"
        )
    return data


def cpp_string(value: str) -> str:
    # JSON string escaping is compatible with the C++ string literals used here.
    return json.dumps(value, ensure_ascii=False)


def generate(zh_cn: dict[str, str], en_us: dict[str, str]) -> str:
    zh_keys = set(zh_cn)
    en_keys = set(en_us)
    if zh_keys != en_keys:
        missing_en = sorted(zh_keys - en_keys)
        missing_zh = sorted(en_keys - zh_keys)
        details = []
        if missing_en:
            details.append(f"missing from en-US.json: {', '.join(missing_en)}")
        if missing_zh:
            details.append(f"missing from zh-CN.json: {', '.join(missing_zh)}")
        raise SystemExit("error: locale key mismatch; " + "; ".join(details))

    lines = [
        "// Generated from i18n/zh-CN.json and i18n/en-US.json. Do not edit.",
        "constexpr Entry kTable[] = {",
    ]
    for key, zh_value in zh_cn.items():
        lines.append(
            f"  {{{cpp_string(key)}, {cpp_string(zh_value)}, "
            f"{cpp_string(en_us[key])}}},"
        )
    lines.extend(["};", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--zh-cn", required=True, type=Path)
    parser.add_argument("--en-us", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    content = generate(load_catalog(args.zh_cn), load_catalog(args.en_us))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == content:
        return
    args.output.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
