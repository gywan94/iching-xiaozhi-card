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
    void InitBatteryWatchdog();
    // 与 m5stack StartUp 一致：BQ27220 未检测到电池时（疑似电池过放），
    // 进入电池提示页 + 深睡 10/20/.../60 秒，时间递增重试。
    void GuardBatteryAtStartup();
    static void ButtonTask(void* arg);
    static void IdleTask(void* arg);
    static void BatteryTask(void* arg);
    void HandleSingleClick();
    void HandleDoubleClick();
    void DoShutdown();
    void OnDivinationClicked(int x, int y);
    void ShowResultPage();

    DayanUi ui_;
    DayanEngine engine_;
    TaskHandle_t button_task_ = nullptr;
    TaskHandle_t idle_task_ = nullptr;
    TaskHandle_t battery_task_ = nullptr;
    std::atomic<int64_t> last_activity_us_{0};
    // 仅未外接充电（电池供电）时生效；有充电/市电时 IdleTask 不进入深睡
    static constexpr int64_t kIdleTimeoutUs = 3LL * 60 * 1000 * 1000; // 3 分钟无操作
    // 低电压保护阈值：电压 <3500mV 且未充电 → 倒计时 5 秒自动关机（同 xiaozhi-card）。
    static constexpr int kLowVoltageMv = 3500;
    static constexpr int kLowVoltageGraceSec = 5;
};
