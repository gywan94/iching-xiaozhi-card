#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

struct BoardDisplayContext {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_touch_handle_t touch = nullptr;
    lv_display_t* lv_display = nullptr;
    lv_indev_t* lv_touch = nullptr;
};

esp_err_t BoardInitDisplayAndTouch(BoardDisplayContext& ctx);
esp_err_t BoardInitStorage();
bool BoardStorageMounted();
const char* BoardLvglFontPath();
int BoardGetBatteryLevelPercent();
// 电池电压 (mV)；BQ27220 不可用或读取失败返回 -1。
int BoardGetBatteryVoltageMv();
// 与 m5stack/xiaozhi-card 一致的 AW32001 充电状态：0=未充电 1=预充 2=充电中 3=充电完成；不可用返回 -1
int BoardGetChargeState();
// 启动时电池过放保护使用：BQ27220 探测到电池则 true。
bool BoardIsGaugePresent();
// 有外部充电/市电等供电（ fuel gauge 非放电）时为 true；无 BQ 或探测失败为 false
bool BoardIsExternalPower();
// 与 xiaozhi-card 一致：使 AW32001 进入运输模式，FET 切断 SYS 轨后设备掉电。
// 正常情况下函数不会返回；若 charger 未就绪，将直接返回不做处理。
void BoardHardShutdown();
