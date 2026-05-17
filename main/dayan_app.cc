#include "dayan_app.h"

#include "board_hal.h"
#include "dayan_data.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

namespace {
constexpr char kTag[] = "dayan_app";
constexpr gpio_num_t kUserButtonGpio = GPIO_NUM_21;
constexpr int kButtonPollMs = 20;
constexpr int kDoubleClickWindowMs = 350;
}

void DayanApp::UpdateActivity() {
    last_activity_us_.store(esp_timer_get_time());
}

void DayanApp::Start() {
    // 上电默认从欢迎页开始，状态机归零，同时重置空闲计时器。
    UpdateActivity();
    engine_.Reset();
    // 所有页面交互都先重置空闲计时器，再执行对应逻辑。
    ui_.on_welcome_confirm = [this]() { UpdateActivity(); ui_.ShowIntro(); };
    ui_.on_intro_start = [this]() {
        UpdateActivity();
        engine_.Reset();
        ui_.ShowDivination();
        ui_.UpdateDivination(engine_);
    };
    ui_.on_divination_click = [this](int x, int y) { UpdateActivity(); OnDivinationClicked(x, y); };
    ui_.on_result_restart = [this]() { UpdateActivity(); ui_.ShowWelcome(); };
    ui_.on_detail_back = [this]() {
        ShowResultPage();
    };

    // 同步构建 UI（LVGL 已就绪），确保 GuardBatteryAtStartup 能立刻使用 SystemTip 提示页。
    lvgl_port_lock(0);
    ui_.Build();
    lvgl_port_unlock();

    // 电池过放保护：BQ27220 探测不到则进低电提示 + 时间递增的深睡重试，与 m5stack StartUp 对齐。
    GuardBatteryAtStartup();

    lv_async_call(
        [](void* user_data) {
            auto* self = static_cast<DayanApp*>(user_data);
            self->ui_.ShowWelcome();
        },
        this);
    InitUserButton();
    InitIdleWatchdog();
    InitBatteryWatchdog();
}

void DayanApp::GuardBatteryAtStartup() {
    if (BoardIsGaugePresent()) {
        return;
    }
    // RTC 内存保留：跨深睡递增睡眠时长，避免短时反复唤醒消耗。
    static RTC_DATA_ATTR int sleep_retry_count = 0;
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
        sleep_retry_count = 0;
    }
    int sleep_sec = 10 + sleep_retry_count * 10;
    if (sleep_sec > 60) {
        sleep_sec = 60;
    }
    ++sleep_retry_count;

    lvgl_port_lock(0);
    ui_.ShowSystemTip("低电量充电中", "请等待");
    lv_refr_now(nullptr);
    lvgl_port_unlock();

    ESP_LOGW(kTag, "battery not detected, deep sleep %d sec then retry", sleep_sec);
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sleep_sec) * 1000000ULL);
    esp_deep_sleep_start();
}

void DayanApp::InitBatteryWatchdog() {
    // 每秒检查一次电池电压：<3.5V 且未充电 → 倒计时 5 秒后关机；充电中只提示不关机。
    xTaskCreatePinnedToCore(BatteryTask, "dayan_bat", 4096, this, 2, &battery_task_, 0);
}

void DayanApp::BatteryTask(void* arg) {
    auto* self = static_cast<DayanApp*>(arg);
    int countdown = kLowVoltageGraceSec;
    bool showing_tip = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        const int volt_mv = BoardGetBatteryVoltageMv();
        if (volt_mv < 0) {
            // BQ27220 暂时读不到（瞬态错误）；跳过本轮，等下次再判定。
            continue;
        }
        if (volt_mv >= kLowVoltageMv) {
            countdown = kLowVoltageGraceSec;
            if (showing_tip) {
                // 电压恢复（用户已插电充电）：从低电提示页切回首页。
                lvgl_port_lock(0);
                self->ui_.ShowWelcome();
                lvgl_port_unlock();
                showing_tip = false;
            }
            continue;
        }
        const int state = BoardGetChargeState();
        if (state > 0) {
            // 充电中：只提示，不关机
            countdown = kLowVoltageGraceSec;
            lvgl_port_lock(0);
            self->ui_.ShowSystemTip("电量低", "充电中…请等待");
            lvgl_port_unlock();
            showing_tip = true;
            continue;
        }
        // 未充电 → 倒计时关机
        char title[32];
        std::snprintf(title, sizeof(title), "电量低 %d 秒后关机", countdown);
        lvgl_port_lock(0);
        self->ui_.ShowSystemTip(title, "请尽快充电");
        lvgl_port_unlock();
        showing_tip = true;
        if (countdown-- <= 0) {
            ESP_LOGW(kTag, "low voltage <%d mV -> shutdown", kLowVoltageMv);
            self->DoShutdown();
            countdown = kLowVoltageGraceSec;
        }
    }
}

void DayanApp::InitUserButton() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << kUserButtonGpio;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    xTaskCreatePinnedToCore(ButtonTask, "dayan_btn", 4096, this, 5, &button_task_, 0);
}

void DayanApp::InitIdleWatchdog() {
    // 仅电池供电时：每 5 秒检查一次，超过 3 分钟无交互则关机（硬关机路径同 xiaozhi-card）；外接/充电不误触关机
    xTaskCreatePinnedToCore(IdleTask, "dayan_idle", 4096, this, 2, &idle_task_, 0);
}

void DayanApp::IdleTask(void* arg) {
    auto* self = static_cast<DayanApp*>(arg);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        const int64_t now = esp_timer_get_time();
        const int64_t last = self->last_activity_us_.load();
        if (now - last >= kIdleTimeoutUs) {
            if (BoardIsExternalPower()) {
                // 接通电源/充电中：不自动深睡
                continue;
            }
            ESP_LOGI(kTag, "idle timeout (battery) -> auto shutdown");
            self->DoShutdown();
        }
    }
}

void DayanApp::DoShutdown() {
    // 严格对齐 m5stack/XiaoZhi-Card 的 Shutdown() 流程：
    // 1) 充电中拒绝关机并提示；2) 同步加载关机页 + 立刻刷新 EPD；3) 进入运输模式。
    // 不再回退到 esp_deep_sleep_start —— shipping mode 失败时直接返回，避免在硬件不切电时
    // 仍以高功耗形式停留在深睡眠，掩盖问题。
    ESP_LOGI(kTag, "Shutdown");

    if (BoardIsExternalPower()) {
        ESP_LOGI(kTag, "充电中不能关机");
        lv_async_call(
            [](void* user_data) {
                static_cast<DayanApp*>(user_data)->ui_.ShowChargingNoShutdownTip();
            },
            this);
        return;
    }

    lvgl_port_lock(0);
    ui_.ShowShutdown();
    lv_refr_now(nullptr);
    lvgl_port_unlock();

    BoardHardShutdown();
}

void DayanApp::ButtonTask(void* arg) {
    auto* self = static_cast<DayanApp*>(arg);
    bool last_pressed = false;
    int click_count = 0;
    int64_t last_release_ms = 0;
    while (true) {
        const bool pressed = gpio_get_level(kUserButtonGpio) == 0;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (pressed && !last_pressed) {
            // press edge
        } else if (!pressed && last_pressed) {
            // release edge
            click_count++;
            last_release_ms = now_ms;
        }
        // 到达双击窗口再结算，避免单击/双击互相误判。
        if (click_count > 0 && (now_ms - last_release_ms) >= kDoubleClickWindowMs) {
            if (click_count >= 2) {
                self->HandleDoubleClick();
            } else {
                self->HandleSingleClick();
            }
            click_count = 0;
        }
        last_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(kButtonPollMs));
    }
}

void DayanApp::HandleSingleClick() {
    ESP_LOGI(kTag, "user button single click -> wake/welcome");
    UpdateActivity();
    lv_async_call(
        [](void* user_data) {
            auto* self = static_cast<DayanApp*>(user_data);
            self->ui_.ShowWelcome();
        },
        this);
}

void DayanApp::HandleDoubleClick() {
    ESP_LOGI(kTag, "user button double click -> shutdown");
    DoShutdown();
}

void DayanApp::OnDivinationClicked(int x, int y) {
    int left_count = 0;
    if (!ui_.ResolveGapClick(x, y, engine_.CurrentTotalSticks(), left_count)) {
        return;
    }
    // 把"缝隙位置"转换为左右分堆数量并推进一变。
    const SplitResult result = engine_.AdvanceWithLeftCount(left_count);
    if (!result.accepted) {
        return;
    }
    ui_.UpdateSplitInfo(result.left, result.right);
    ESP_LOGI(
        kTag,
        "click=%d total=%d xy=(%d,%d) left=%d right=%d line=%d change=%d",
        result.click_index,
        result.total_sticks,
        x,
        y,
        result.left,
        result.right,
        result.line_index,
        result.change_index);

    if (engine_.IsFinished()) {
        ShowResultPage();
        return;
    }
    ui_.UpdateDivination(engine_);
}

void DayanApp::ShowResultPage() {
    GuaDetails details;
    // 离线表未覆盖时给出明确提示，便于你直接定位到生成脚本。
    if (!GetBookGuaDetails(engine_.GetRawLineCode(), details)) {
        details.guayao = engine_.GetRawLineCode();
        details.getgua = "数据未命中";
        details.g_gua = "数据未命中";
        details.yao_results = "{}";
        details.explaination2 = "[]";
        details.origin_name = "数据未命中";
        details.changed_name = "数据未命中";
        details.yao_text = "当前离线字典尚未覆盖该卦码，请运行 tools/gen_iching_data.py 生成完整64卦数据。";
        details.changed_yao = "请补齐 generated/iching_data.inc";
    }
    ui_.ShowResult(details, engine_);
}
