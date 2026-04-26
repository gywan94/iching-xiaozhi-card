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
