# 把 35 个在 font_puhui_16_1.c 中缺失的易经汉字追加生成后合并
# 用法：直接运行此脚本（在 claw-card 根目录）

$MISSING = "刲卼咥嗃夬姤愬扐揜柅洟甃畬禴稊窞繘胏脢臲茀菑蔀藟虩衎袽豮輹遯邅鞶頄颙﹔"

Set-Location "c:\Users\william\cursorproject\claw-card"

node node_modules/.bin/lv_font_conv.js `
    --force-fast-kern-format `
    --no-compress `
    --no-prefilter `
    --font "xiaozhi-claw/components/xiaozhi-fonts/AlibabaPuHuiTi-3-55-Regular.ttf" `
    --format lvgl `
    --lv-include lvgl.h `
    --bpp 1 `
    --size 16 `
    --symbols $MISSING `
    -o "xiaozhi-dayan-app/tools/font_missing_patch.c"

Write-Host "Done. Check xiaozhi-dayan-app/tools/font_missing_patch.c"
