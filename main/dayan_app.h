#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

#include "dayan_engine.h"
#include "dayan_ui.h"

class DayanApp {
public:
    void Start();

    // 任意用户交互（触屏/按键）调用此函数重置空闲计时器。
    void UpdateActivity();

private:
    void InitUserButton();
    void InitIdleWatchdog();
    static void ButtonTask(void* arg);
    static void IdleTask(void* arg);
    void HandleSingleClick();
    void HandleDoubleClick();
    void DoShutdown();
    void OnDivinationClicked(int x, int y);
    void ShowResultPage();

    DayanUi ui_;
    DayanEngine engine_;
    TaskHandle_t button_task_ = nullptr;
    TaskHandle_t idle_task_ = nullptr;
    std::atomic<int64_t> last_activity_us_{0};
    static constexpr int64_t kIdleTimeoutUs = 60LL * 1000 * 1000; // 60 秒
};
