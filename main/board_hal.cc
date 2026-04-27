#include "board_hal.h"

#include "driver/i2c_master.h"
#include "driver/spi_common.h"
#include "esp_epd_gdey027t91.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "bq27220.h"
#include "aw32001.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

namespace {
constexpr char kTag[] = "dayan_board";

constexpr int kDisplayWidth = 176;
constexpr int kDisplayHeight = 264;

constexpr spi_host_device_t kSpiHost = SPI2_HOST;
constexpr gpio_num_t kPinSck = GPIO_NUM_45;
constexpr gpio_num_t kPinMosi = GPIO_NUM_46;
constexpr gpio_num_t kPinMiso = GPIO_NUM_13;
constexpr gpio_num_t kPinDc = GPIO_NUM_41;
constexpr gpio_num_t kPinCs = GPIO_NUM_42;
constexpr gpio_num_t kPinRst = GPIO_NUM_47;

constexpr gpio_num_t kPinI2cSda = GPIO_NUM_2;
constexpr gpio_num_t kPinI2cScl = GPIO_NUM_1;
constexpr gpio_num_t kTouchInt = GPIO_NUM_16;
constexpr gpio_num_t kSdCs = GPIO_NUM_14;
constexpr char kSdMountPoint[] = "/sdcard";
constexpr char kLvglFontPath[] = "A:/sdcard/fonts/font_puhui_16_1.bin";
constexpr uint8_t kAw32001Address = 0x49;

bool g_sd_mounted = false;
i2c_master_bus_handle_t g_i2c_bus = nullptr;
Bq27220* g_gauge = nullptr;
Aw32001* g_charger = nullptr;
}  // namespace

esp_err_t BoardInitDisplayAndTouch(BoardDisplayContext& ctx) {
    // EPD 与 SD 共用 SPI2，总线先初始化，后续屏幕和存储都复用。
    spi_bus_config_t spi_cfg = {};
    spi_cfg.sclk_io_num = kPinSck;
    spi_cfg.mosi_io_num = kPinMosi;
    spi_cfg.miso_io_num = kPinMiso;
    spi_cfg.quadwp_io_num = GPIO_NUM_NC;
    spi_cfg.quadhd_io_num = GPIO_NUM_NC;
    spi_cfg.max_transfer_sz = kDisplayWidth * kDisplayHeight;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(kSpiHost, &spi_cfg, SPI_DMA_CH_AUTO), kTag, "spi init failed");

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = kPinDc;
    io_cfg.cs_gpio_num = kPinCs;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.spi_mode = 0;
    io_cfg.trans_queue_depth = 8;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)kSpiHost, &io_cfg, &ctx.panel_io),
        kTag,
        "new panel io failed");

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = kPinRst;
    panel_cfg.bits_per_pixel = 1;
    panel_cfg.rgb_endian = LCD_RGB_ENDIAN_BGR;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gdey027t91(ctx.panel_io, &panel_cfg, &ctx.panel), kTag, "new panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(ctx.panel), kTag, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(ctx.panel), kTag, "panel init failed");
    // 仿照 xiaozhi-card：开机先全黑再全白，降低首屏残影。
    uint8_t* buf = static_cast<uint8_t*>(heap_caps_malloc(kDisplayWidth * kDisplayHeight, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
    if (buf != nullptr) {
        memset(buf, 0x00, kDisplayWidth * kDisplayHeight);
        panel_gdey027t91_draw_bitmap_full(ctx.panel, 0, 0, kDisplayWidth, kDisplayHeight, buf);
        vTaskDelay(pdMS_TO_TICKS(250));
        memset(buf, 0xFF, kDisplayWidth * kDisplayHeight);
        panel_gdey027t91_draw_bitmap_full(ctx.panel, 0, 0, kDisplayWidth, kDisplayHeight, buf);
        vTaskDelay(pdMS_TO_TICKS(250));
        free(buf);
    }

    // I2C 同时用于触摸与电量计芯片。
    i2c_master_bus_config_t i2c_cfg = {};
    i2c_cfg.i2c_port = I2C_NUM_0;
    i2c_cfg.sda_io_num = kPinI2cSda;
    i2c_cfg.scl_io_num = kPinI2cScl;
    i2c_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_cfg.glitch_ignore_cnt = 7;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_cfg, &g_i2c_bus), kTag, "i2c init failed");

    esp_lcd_panel_io_handle_t touch_io = nullptr;
    esp_lcd_panel_io_i2c_config_t touch_io_cfg = {};
    touch_io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
    touch_io_cfg.control_phase_bytes = 1;
    touch_io_cfg.dc_bit_offset = 0;
    touch_io_cfg.lcd_cmd_bits = 8;
    touch_io_cfg.flags.disable_control_phase = 1;
    touch_io_cfg.scl_speed_hz = 100000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(g_i2c_bus, &touch_io_cfg, &touch_io), kTag, "touch io failed");

    esp_lcd_touch_config_t touch_cfg = {
        .x_max = kDisplayWidth,
        .y_max = kDisplayHeight,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = kTouchInt,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        .user_data = nullptr,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(touch_io, &touch_cfg, &ctx.touch), kTag, "touch init failed");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16 * 1024;
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    port_cfg.task_affinity = 1;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), kTag, "lvgl init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = ctx.panel_io,
        .panel_handle = ctx.panel,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(kDisplayWidth * kDisplayHeight),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(kDisplayWidth),
        .vres = static_cast<uint32_t>(kDisplayHeight),
        .monochrome = false,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = false,
                .mirror_y = false,
            },
        .color_format = LV_COLOR_FORMAT_L8,
        .flags =
            {
                .buff_dma = 0,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .full_refresh = 1,
                .direct_mode = 0,
            },
    };
    ctx.lv_display = lvgl_port_add_disp(&disp_cfg);
    if (ctx.lv_display == nullptr) {
        ESP_LOGE(kTag, "lv display add failed");
        return ESP_FAIL;
    }

    const lvgl_port_touch_cfg_t lv_touch_cfg = {
        .disp = ctx.lv_display,
        .handle = ctx.touch,
    };
    ctx.lv_touch = lvgl_port_add_touch(&lv_touch_cfg);
    if (ctx.lv_touch == nullptr) {
        ESP_LOGE(kTag, "lv touch add failed");
        return ESP_FAIL;
    }
    if (g_gauge == nullptr && g_i2c_bus != nullptr) {
        // 电量计仅初始化一次，供首页实时电量显示使用。
        g_gauge = new Bq27220(g_i2c_bus, BQ27220_I2C_ADDRESS);
        if (!g_gauge->detect()) {
            ESP_LOGW(kTag, "bq27220 detect failed");
        }
    }
    if (g_charger == nullptr && g_i2c_bus != nullptr) {
        const esp_err_t probe_ret = i2c_master_probe(g_i2c_bus, kAw32001Address, pdMS_TO_TICKS(100));
        if (probe_ret == ESP_OK) {
            g_charger = new Aw32001(g_i2c_bus, kAw32001Address);
            // 与 xiaozhi-card 初始化策略对齐
            g_charger->SetShippingMode(false);               // 关闭运输模式
            g_charger->SetNtcFunction(false);                // 未使用 NTC
            g_charger->SetDischargeCurrent(2800);            // 最大放电电流
            g_charger->SetChargeCurrent(260);                // 最大充电电流
            g_charger->SetChargeVoltage(4200);               // 满电电压 4.2V
            g_charger->SetPreChargeCurrent(31);              // 预充电电流
            g_charger->SetPrechargeToFastchargeThreshold(0); // 预充转快充阈值
            g_charger->SetCharge(true);                      // 开启充电
            ESP_LOGI(kTag, "aw32001 charger detected");
        } else {
            ESP_LOGW(kTag, "aw32001 not found, hard shutdown disabled");
        }
    }
    return ESP_OK;
}

esp_err_t BoardInitStorage() {
    if (g_sd_mounted) {
        return ESP_OK;
    }
    // 通过 SDSPI 挂载 80M 存储到 /sdcard，给 LVGL binfont 动态读取。
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = kSpiHost;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = kSdCs;
    slot_config.host_id = kSpiHost;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_card_t* card = nullptr;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(kSdMountPoint, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGW(kTag, "mount %s failed: %s", kSdMountPoint, esp_err_to_name(ret));
        return ret;
    }
    g_sd_mounted = true;
    if (card != nullptr) {
        sdmmc_card_print_info(stdout, card);
    }
    ESP_LOGI(kTag, "storage mounted at %s", kSdMountPoint);
    return ESP_OK;
}

bool BoardStorageMounted() {
    return g_sd_mounted;
}

const char* BoardLvglFontPath() {
    // A: 是 LVGL 的 stdio 盘符映射（CONFIG_LV_FS_STDIO_LETTER=65）。
    return kLvglFontPath;
}

int BoardGetBatteryLevelPercent() {
    // 返回 0~100；读取失败返回 -1 供 UI 显示“不可用”状态。
    if (g_gauge == nullptr || !g_gauge->detect()) {
        return -1;
    }
    const uint16_t raw = g_gauge->getChargePcnt();
    if (raw > 100) {
        return 100;
    }
    return static_cast<int>(raw);
}

bool BoardIsExternalPower() {
    // 与 xiaozhi-card 一致：优先依据 AW32001 充电状态判断是否外接供电
    if (g_charger != nullptr) {
        return g_charger->GetChargeState() != 0;
    }
    // 回退：若无 AW32001，再用 BQ27220 状态近似判断
    if (g_gauge == nullptr || !g_gauge->detect()) {
        return false;
    }
    return g_gauge->getIsCharging();
}

bool BoardHardShutdown() {
    if (g_charger == nullptr) {
        return false;
    }
    // 与 xiaozhi-card 一致：进入 AW32001 运输模式实现硬关机。
    g_charger->SetShippingMode(true);
    vTaskDelay(pdMS_TO_TICKS(3000));
    return true;
}
