#include "dayan_app.h"

#include "board_hal.h"
#include "dayan_data.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "dayan_app";
constexpr gpio_num_t kUserButtonGpio = GPIO_NUM_21;
constexpr int kButtonPollMs = 20;
constexpr int kDoubleClickWindowMs = 350;
constexpr int kShutdownDelayMs = 1200;
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

    lv_async_call(
        [](void* user_data) {
            auto* self = static_cast<DayanApp*>(user_data);
            self->ui_.Build();
            self->ui_.ShowWelcome();
        },
        this);
    InitUserButton();
    InitIdleWatchdog();
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
    // 仅电池供电时：每 5 秒检查一次，超过 60 秒无交互则深睡关机；外接/充电不自动关机（与 xiaozhi-card 一致）
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
    if (BoardIsExternalPower()) {
        ESP_LOGI(kTag, "充电/外接电源，不进入深睡关机（同 xiaozhi-card 充电中不能关机）");
        lv_async_call(
            [](void* user_data) {
                static_cast<DayanApp*>(user_data)->ui_.ShowChargingNoShutdownTip();
            },
            this);
        return;
    }
    // 显示关机页面，留足时间让墨水屏刷新完，再进入深睡眠。
    lv_async_call(
        [](void* user_data) {
            auto* self = static_cast<DayanApp*>(user_data);
            self->ui_.ShowShutdown();
        },
        this);
    vTaskDelay(pdMS_TO_TICKS(kShutdownDelayMs));
    if (BoardHardShutdown()) {
        // 正常情况下硬关机会掉电，不会返回；返回表示芯片不支持或执行失败。
        ESP_LOGW(kTag, "hard shutdown did not power off, fallback to deep sleep");
    }
    esp_sleep_enable_ext1_wakeup(BIT64(kUserButtonGpio), ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
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
