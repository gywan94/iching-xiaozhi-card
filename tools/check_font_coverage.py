#!/usr/bin/env python3
"""
检查第二页、第四页所有汉字是否被 font_puhui_16_1.c 覆盖。
输出缺失字符列表，供 lv_font_conv 补字使用。
"""
import re
import json
from pathlib import Path

ROOT = Path("c:/Users/william/cursorproject/claw-card")
FONT_C = ROOT / "xiaozhi-claw/components/xiaozhi-fonts/src/font_puhui_16_1.c"
ICHING_INC = ROOT / "xiaozhi-dayan-app/main/generated/iching_data.inc"

# ── 1. 第二页静态文本 ──────────────────────────────────────
PAGE2_TEXT = (
    "大衍之数五十，其用四十有九。分而为二以像两，挂一以像三，"
    "揲之以四以象四时，归奇于扐以象闰；五岁再闰，故再扐而后挂。"
    "十有八变而成卦。开始占卜"
)

# ── 2. 第四页所有文本（来自 iching_data.inc 的全量卦爻辞）───
def extract_iching_chars():
    text = ICHING_INC.read_text(encoding="utf-8")
    # 从 JSON 字符串段提取所有可见字符
    # inc 文件每行形如 {"000000", {"坤", "...json..."}}
    all_chars = ""
    for line in text.splitlines():
        m = re.search(r'\{"[01]{6}", \{"(.+?)", "(\[.*)', line)
        if m:
            name_part = m.group(1)
            json_part = m.group(2).rstrip('"}},')
            all_chars += name_part
            try:
                obj = json.loads(json_part)
                def walk(o):
                    if isinstance(o, str):
                        return o
                    if isinstance(o, list):
                        return "".join(walk(i) for i in o)
                    if isinstance(o, dict):
                        return "".join(walk(v) for v in o.values())
                    return ""
                all_chars += walk(obj)
            except Exception:
                all_chars += json_part
    return all_chars

# ── 3. 解析字库已有的 Unicode 码点 ───────────────────────────
def parse_font_unicodes(font_c_path: Path):
    text = font_c_path.read_text(encoding="utf-8", errors="ignore")
    covered = set()

    # 连续段：LV_FONT_DECLARE 中的 cmaps 含有
    # unicode_list_N[] = { 0x1234, 0x5678, ... }
    for m in re.finditer(r'unicode_list_\w+\[\]\s*=\s*\{([^}]+)\}', text):
        for val in re.findall(r'0x([0-9A-Fa-f]+)', m.group(1)):
            covered.add(int(val, 16))

    # 连续范围段：range_start, range_length
    for m in re.finditer(r'\.range_start\s*=\s*0x([0-9A-Fa-f]+).*?\.range_length\s*=\s*(\d+)', text, re.DOTALL):
        start = int(m.group(1), 16)
        length = int(m.group(2))
        for cp in range(start, start + length):
            covered.add(cp)

    return covered

# ── 4. 找出缺失字符 ──────────────────────────────────────────
def main():
    iching_chars = extract_iching_chars()
    all_text = PAGE2_TEXT + iching_chars

    # 只关心 CJK / 常用汉字范围
    all_chars = set(ch for ch in all_text if ord(ch) > 0x2E7F)

    covered = parse_font_unicodes(FONT_C)

    missing = sorted(ch for ch in all_chars if ord(ch) not in covered)

    print(f"总计不重复汉字数: {len(all_chars)}")
    print(f"字库已覆盖码点数: {len(covered)}")
    print(f"缺失字符数: {len(missing)}")
    if missing:
        print("\n缺失字符（可直接复制给 lv_font_conv --symbols）：")
        print("".join(missing))
        print("\n缺失字符 Unicode 码点：")
        for ch in missing:
            print(f"  U+{ord(ch):04X}  {ch}")
    else:
        print("\n✅ 所有字符均在字库覆盖范围内！")

if __name__ == "__main__":
    main()
