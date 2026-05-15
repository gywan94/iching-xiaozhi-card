#pragma once

#include <functional>
#include <vector>

#include "dayan_data.h"
#include "dayan_engine.h"
#include "lvgl.h"

class DayanUi {
public:
    void Build();
    void ShowWelcome();
    void ShowIntro();
    void ShowDivination();
    void UpdateDivination(const DayanEngine& engine);
    void UpdateSplitInfo(int left_count, int right_count);
    void ShowResult(const GuaDetails& details, const DayanEngine& engine);
    void ShowShutdown();
    // 绘制卦象图：在指定父对象上绘制六爻卦象
    // yao_values: 6个爻值(6/7/8/9)，index 0=初爻(bottom)，5=上爻(top)
    // x, y: 绘制位置
    // width: 卦象宽度
    void DrawHexagram(lv_obj_t* parent, const int yao_values[6], int x, int y, int width);
    // 显示卦详情页
    void ShowGuaDetail(const char* gua_name, const std::string& gua_content);
    // 与 xiaozhi-card 一致：充电/外接电源时无法关机，提示后自动消失
    void ShowChargingNoShutdownTip();
    void UpdateWelcomeBattery();

    std::function<void()> on_welcome_confirm;
    std::function<void()> on_intro_start;
    std::function<void(int x, int y)> on_divination_click;
    std::function<void()> on_result_restart;
    std::function<void()> on_detail_back;

    bool ResolveGapClick(int x, int y, int stick_count, int& out_left_count) const;

private:
    const lv_font_t* ResolveTextFont();
    void RebuildSticks(int stick_count);
    void EnsureStickPool();
    static void OnWelcomeBtn(lv_event_t* e);
    static void OnIntroBtn(lv_event_t* e);
    static void OnDivinationTouch(lv_event_t* e);
    static void OnResultBtn(lv_event_t* e);
    static void OnBenguaClick(lv_event_t* e);
    static void OnBianguaClick(lv_event_t* e);
    static void OnDetailBack(lv_event_t* e);

    lv_obj_t* scr_welcome_ = nullptr;
    lv_obj_t* scr_intro_ = nullptr;
    lv_obj_t* scr_divination_ = nullptr;
    lv_obj_t* scr_result_ = nullptr;
    lv_obj_t* scr_shutdown_ = nullptr;
    lv_obj_t* scr_detail_ = nullptr;
    lv_obj_t* welcome_battery_label_ = nullptr;
    lv_obj_t* welcome_battery_icon_ = nullptr;

    lv_obj_t* label_progress_ = nullptr;
    lv_obj_t* progress_scroll_ = nullptr;
    lv_obj_t* label_split_info_ = nullptr;
    lv_obj_t* frame_box_ = nullptr;
    lv_obj_t* stick_layer_ = nullptr;
    lv_obj_t* result_scroll_ = nullptr;
    lv_obj_t* result_text_ = nullptr;
    lv_obj_t* result_hex_area_ = nullptr;
    lv_obj_t* bengua_name_label_ = nullptr;
    lv_obj_t* biangua_name_label_ = nullptr;
    lv_obj_t* raw_yao_label_ = nullptr;  // 原始爻值显示
    lv_obj_t* detail_scroll_ = nullptr;
    lv_obj_t* detail_text_ = nullptr;
    std::vector<lv_obj_t*> stick_objs_;
    std::vector<lv_obj_t*> hexagram_objs_;
    // 存储当前卦象信息用于详情页
    std::string current_bengua_name_;
    std::string current_biangua_name_;
    std::string current_bengua_content_;
    std::string current_biangua_content_;
    lv_font_t* sd_font_ = nullptr;
    const lv_font_t* text_font_ = nullptr;
    bool first_welcome_paint_done_ = false;
    // 可写字体副本：将补丁字库链为 puhui 的 fallback，避免写 flash 只读区。
    lv_font_t combined_font_ = {};
    // 存储当前爻值用于详情页绘制
    int current_yao_values_[6] = {0};
    int current_changed_yao_values_[6] = {0};
};
