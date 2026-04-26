#!/usr/bin/env python3
import json
import sys
import types
from pathlib import Path

if "sxtwl" not in sys.modules:
    fake_sxtwl = types.ModuleType("sxtwl")

    def _unused_from_solar(*_args, **_kwargs):
        raise RuntimeError("fromSolar is unavailable in data generation mode")

    fake_sxtwl.fromSolar = _unused_from_solar
    sys.modules["sxtwl"] = fake_sxtwl

if "ephem" not in sys.modules:
    fake_ephem = types.ModuleType("ephem")

    class _Date:
        def __init__(self, *_args, **_kwargs):
            raise RuntimeError("Date is unavailable in data generation mode")

    fake_ephem.Date = _Date
    sys.modules["ephem"] = fake_ephem

from ichingshifa import ichingshifa


def main() -> None:
    out = Path("c:/Users/william/cursorproject/claw-card/xiaozhi-dayan-app/main/generated")
    out.mkdir(parents=True, exist_ok=True)

    provider = ichingshifa.Iching()
    table = {}
    for n in range(64):
        bits = f"{n:06b}"
        code = "".join("7" if c == "1" else "8" for c in bits)
        detail = provider.mget_bookgua_details(code)
        name = detail[1] if isinstance(detail, list) and len(detail) > 1 else bits
        table[bits] = {
            "name": name,
            "text": json.dumps(detail, ensure_ascii=False),
        }

    with (out / "iching_data.json").open("w", encoding="utf-8") as f:
        json.dump(table, f, ensure_ascii=False, indent=2)

    lines = ["static const std::unordered_map<std::string, Entry> kIChingData = {"]
    for code in sorted(table):
        name = table[code]["name"].replace("\\", "\\\\").replace("\"", "\\\"")
        text = table[code]["text"].replace("\\", "\\\\").replace("\"", "\\\"")
        lines.append(f"    {{\"{code}\", {{\"{name}\", \"{text}\"}}}},")
    lines.append("};")

    with (out / "iching_data.inc").open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print("generated:", out / "iching_data.json")
    print("generated:", out / "iching_data.inc")


if __name__ == "__main__":
    main()
