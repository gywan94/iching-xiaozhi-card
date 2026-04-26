#!/usr/bin/env python3
"""
从 ichingshifa 生成固件查表文件:
- main/generated/iching_data.json
- main/generated/iching_data.inc
"""

import argparse
import itertools
import json
from pathlib import Path


def all_codes():
    for bits in itertools.product("01", repeat=6):
        yield "".join(bits)


def load_provider(repo_path: Path):
    import importlib.util
    module_path = repo_path / "ichingshifa.py"
    spec = importlib.util.spec_from_file_location("ichingshifa", str(module_path))
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ichingshifa-repo", required=True, help="ichingshifa 仓库路径")
    parser.add_argument("--output-dir", required=True, help="输出目录，如 main/generated")
    args = parser.parse_args()

    provider = load_provider(Path(args.ichingshifa_repo))
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    table = {}
    for code in all_codes():
        detail = provider.mget_bookgua_details(code)
        table[code] = {
            "name": detail.get("bookgua", code) if isinstance(detail, dict) else code,
            "text": json.dumps(detail, ensure_ascii=False),
        }

    with (output_dir / "iching_data.json").open("w", encoding="utf-8") as f:
        json.dump(table, f, ensure_ascii=False, indent=2)

    lines = [
        "static const std::unordered_map<std::string, Entry> kIChingData = {",
    ]
    for code, item in sorted(table.items()):
        name = item["name"].replace('"', '\\"')
        text = item["text"].replace('"', '\\"')
        lines.append(f'    {{"{code}", {{"{name}", "{text}"}}}},')
    lines.append("};")
    with (output_dir / "iching_data.inc").open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print("generated:", output_dir / "iching_data.json")
    print("generated:", output_dir / "iching_data.inc")


if __name__ == "__main__":
    main()
