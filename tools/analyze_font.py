#!/usr/bin/env python3
import re

FONT = "c:/Users/william/cursorproject/claw-card/xiaozhi-claw/components/xiaozhi-fonts/src/font_puhui_16_1.c"

f = open(FONT, encoding='utf-8', errors='ignore').read()

# 1. 注释里的 U+XXXX 字符（最可靠的方式）
cjk_comments = re.findall(r'/\* U\+([0-9A-Fa-f]{4,6})', f)
cjk_codepoints = set()
for h in cjk_comments:
    cp = int(h, 16)
    if cp >= 0x4E00:
        cjk_codepoints.add(cp)
print(f"CJK glyphs in font (by comment): {len(cjk_codepoints)}")

# 列出所有CJK字符
all_cps = sorted(int(h, 16) for h in cjk_comments if int(h, 16) >= 0x4E00)
print(f"CJK range: U+{all_cps[0]:04X} ~ U+{all_cps[-1]:04X}" if all_cps else "none")

# 2. 现在检查第二页和第四页需要哪些字
import json, sys
sys.path.insert(0, '.')

PAGE2 = (
    "大衍之数五十，其用四十有九。分而为二以像两，挂一以像三，"
    "揲之以四以象四时，归奇于扐以象闰；五岁再闰，故再扐而后挂。"
    "十有八变而成卦。开始占卜"
)

INC = "c:/Users/william/cursorproject/claw-card/xiaozhi-dayan-app/main/generated/iching_data.inc"
inc_text = open(INC, encoding='utf-8').read()

# 用更简单的方式提取：去掉转义，提取所有汉字
raw = inc_text.replace('\\n', '').replace('\\"', '"')
all_text = PAGE2 + raw

needed = set(ch for ch in all_text if ord(ch) >= 0x4E00)
print(f"Total unique CJK chars needed: {len(needed)}")

missing = sorted(ch for ch in needed if ord(ch) not in cjk_codepoints)
print(f"Missing from font: {len(missing)}")
if missing:
    print("Missing chars:")
    print("".join(missing))
    print("\nUnicode list:")
    for ch in missing:
        print(f"  U+{ord(ch):04X}  {ch}")
else:
    print("All covered!")
