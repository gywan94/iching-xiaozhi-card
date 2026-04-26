#include "board_hal.h"
#include "dayan_app.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

extern "C" void app_main(void) {
    // 先初始化 NVS；版本变更或页损坏时自动擦除重建，避免启动失败。
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    BoardDisplayContext board_ctx;
    // 初始化屏幕、触摸、LVGL，以及电量计（BQ27220）探测。
    ESP_ERROR_CHECK(BoardInitDisplayAndTouch(board_ctx));
    // 挂载 80M 存储到 /sdcard，供运行时动态加载字库。
    esp_err_t storage_err = BoardInitStorage();
    if (storage_err != ESP_OK) {
        ESP_LOGW("dayan_main", "storage not ready, fallback to built-in font");
    }

    static DayanApp app;
    app.Start();
    ESP_LOGI("dayan_main", "dayan app started");
}
