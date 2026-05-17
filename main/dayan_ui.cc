#include "dayan_ui.h"
#include "dayan_data.h"
#include "board_hal.h"
#include "font_awesome_symbols.h"
#include "esp_epd_gdey027t91.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

LV_FONT_DECLARE(font_puhui_16_1);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_iching_patch_16_1);
LV_IMAGE_DECLARE(shutdown_image);

namespace {
constexpr int kFrameW = 170;
constexpr int kFrameH = 200;
constexpr int kStickSidePadding = 4;
constexpr int kStickTopPadding = 4;
constexpr int kMaxSticks = 49;
}  // namespace

void DayanUi::Build() {
    // 把补丁字库（35个冷僻易经字）设为 puhui 主字库的后备链：
    // puhui 找不到字形 → 自动到 patch 里取。
    // 做法：把 font_iching_patch_16_1（const，在 flash）复制到可写成员
    // combined_font_，再把 font_puhui_16_1 挂到 combined_font_.fallback。
    // combined_font_ 作为最终使用的内置字体，渲染时先查 patch，再查 puhui。
    combined_font_ = font_iching_patch_16_1;          // 复制 struct 到可写内存
    combined_font_.fallback = &font_puhui_16_1;        // 挂 puhui 为 fallback

    // 字体策略：优先从 SD 动态加载，失败则回退内置合并字库。
    text_font_ = ResolveTextFont();
    scr_welcome_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_welcome_, text_font_, 0);
    // 首页顶部状态区：仿 xiaozhi-card 风格显示电量信息。
    lv_obj_t* welcome_status = lv_obj_create(scr_welcome_);
    lv_obj_set_size(welcome_status, 170, 24);
    lv_obj_align(welcome_status, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_opa(welcome_status, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(welcome_status, 0, 0);
    lv_obj_set_style_pad_all(welcome_status, 0, 0);
    lv_obj_clear_flag(welcome_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* battery_title = lv_label_create(welcome_status);
    lv_obj_set_style_text_font(battery_title, text_font_, 0);
    lv_label_set_text(battery_title, "电量");
    lv_obj_align(battery_title, LV_ALIGN_LEFT_MID, 0, 0);
    welcome_battery_icon_ = lv_label_create(welcome_status);
    lv_obj_set_style_text_font(welcome_battery_icon_, &font_awesome_16_4, 0);
    lv_label_set_text(welcome_battery_icon_, FONT_AWESOME_BATTERY_FULL);
    lv_obj_align(welcome_battery_icon_, LV_ALIGN_RIGHT_MID, -46, 0);
    welcome_battery_label_ = lv_label_create(welcome_status);
    lv_obj_set_style_text_font(welcome_battery_label_, text_font_, 0);
    lv_label_set_text(welcome_battery_label_, "--%");
    lv_obj_align(welcome_battery_label_, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_style_bg_color(scr_welcome_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_welcome_, lv_color_black(), 0);
    lv_obj_t* title = lv_label_create(scr_welcome_);
    lv_obj_set_style_text_font(title, text_font_, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_label_set_text(title, "大衍筮法");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t* text = lv_label_create(scr_welcome_);
    lv_obj_set_style_text_font(text, text_font_, 0);
    lv_label_set_text(text, "请默默想所占卜问题");
    lv_obj_align(text, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_t* btn = lv_btn_create(scr_welcome_);
    lv_obj_set_size(btn, 90, 38);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn, OnWelcomeBtn, LV_EVENT_CLICKED, this);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_obj_set_style_text_font(btn_label, text_font_, 0);
    lv_label_set_text(btn_label, "确认");
    lv_obj_center(btn_label);

    scr_intro_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_intro_, text_font_, 0);
    lv_obj_set_style_bg_color(scr_intro_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_intro_, lv_color_black(), 0);
    lv_obj_clear_flag(scr_intro_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_intro_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t* intro = lv_label_create(scr_intro_);
    lv_obj_set_style_text_font(intro, text_font_, 0);
    lv_label_set_long_mode(intro, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(intro, 164, 180);
    lv_obj_align(intro, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_text(
        intro,
        "大衍之数五十，其用四十有九。分而为二以像两，挂一以像三，揲之以四以象四时，归奇于扐以象闰；"
        "五岁再闰，故再扐而后挂。十有八变而成卦。");
    lv_obj_t* btn_intro = lv_btn_create(scr_intro_);
    lv_obj_set_size(btn_intro, 108, 38);
    lv_obj_align(btn_intro, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn_intro, OnIntroBtn, LV_EVENT_CLICKED, this);
    lv_obj_t* intro_btn_label = lv_label_create(btn_intro);
    lv_obj_set_style_text_font(intro_btn_label, text_font_, 0);
    lv_label_set_text(intro_btn_label, "开始占卜");
    lv_obj_center(intro_btn_label);

    scr_divination_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_divination_, text_font_, 0);
    lv_obj_set_style_bg_color(scr_divination_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_divination_, lv_color_black(), 0);
    lv_obj_clear_flag(scr_divination_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_divination_, LV_SCROLLBAR_MODE_OFF);
    progress_scroll_ = lv_obj_create(scr_divination_);
    lv_obj_set_size(progress_scroll_, 168, 22);
    lv_obj_align(progress_scroll_, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(progress_scroll_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(progress_scroll_, 0, 0);
    lv_obj_set_style_pad_all(progress_scroll_, 0, 0);
    lv_obj_set_scroll_dir(progress_scroll_, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(progress_scroll_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(progress_scroll_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(progress_scroll_, LV_OBJ_FLAG_CLICKABLE);
    label_progress_ = lv_label_create(progress_scroll_);
    lv_obj_set_style_text_font(label_progress_, text_font_, 0);
    lv_obj_set_width(label_progress_, 168);
    lv_label_set_long_mode(label_progress_, LV_LABEL_LONG_CLIP);
    lv_obj_clear_flag(label_progress_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(label_progress_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(label_progress_, "这是第1次点击");
    label_split_info_ = lv_label_create(scr_divination_);
    lv_obj_set_style_text_font(label_split_info_, text_font_, 0);
    lv_obj_set_width(label_split_info_, 168);
    lv_obj_align(label_split_info_, LV_ALIGN_TOP_MID, 0, 34);
    lv_label_set_text(label_split_info_, "左边0根，右边49根");
    frame_box_ = lv_obj_create(scr_divination_);
    lv_obj_set_size(frame_box_, kFrameW, kFrameH);
    lv_obj_align(frame_box_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_opa(frame_box_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(frame_box_, lv_color_black(), 0);
    lv_obj_set_style_border_width(frame_box_, 2, 0);
    lv_obj_set_style_border_opa(frame_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(frame_box_, 0, 0);
    lv_obj_set_style_pad_all(frame_box_, 0, 0);
    lv_obj_clear_flag(frame_box_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(frame_box_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(frame_box_, OnDivinationTouch, LV_EVENT_PRESSED, this);
    stick_layer_ = lv_obj_create(frame_box_);
    lv_obj_set_size(stick_layer_, kFrameW, kFrameH);
    lv_obj_center(stick_layer_);
    lv_obj_set_style_bg_opa(stick_layer_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stick_layer_, 0, 0);
    lv_obj_set_style_pad_all(stick_layer_, 0, 0);
    lv_obj_clear_flag(stick_layer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(stick_layer_, LV_OBJ_FLAG_CLICKABLE);

    scr_result_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_result_, text_font_, 0);
    lv_obj_set_style_bg_color(scr_result_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_result_, lv_color_black(), 0);
    lv_obj_clear_flag(scr_result_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_result_, LV_SCROLLBAR_MODE_OFF);

    // ========== 结果页布局 ==========
    // 屏幕: 176x264
    //
    // ┌─────────────────────────────┐
    // │ 本卦名  [图] → 变卦名 [图]  │  y: 0~32 (顶部区域)
    // ├─────────────────────────────┤
    // │ 原始爻值: 787978             │
    // │ 【本卦】乾                   │
    // │ 本卦爻辞...                  │  y: 34~228 (文本区域 194px)
    // │ 【变卦】坤                   │
    // │ 变卦爻辞...                  │
    // │ 断语内容...                  │
    // ├─────────────────────────────┤
    // │      [重新起卦]             │  y: 230~262
    // └─────────────────────────────┘

    // 顶部区域: 卦名+卦象 (y: 0~32)
    result_hex_area_ = lv_obj_create(scr_result_);
    lv_obj_set_size(result_hex_area_, 174, 32);
    lv_obj_align(result_hex_area_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(result_hex_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(result_hex_area_, 0, 0);
    lv_obj_set_style_pad_all(result_hex_area_, 0, 0);
    lv_obj_clear_flag(result_hex_area_, LV_OBJ_FLAG_SCROLLABLE);

    // 第一行四等分布局 (屏幕宽176，每格44)
    // 第1格(0-43): 本卦名 | 第2格(44-87): 本卦图 | 第3格(88-131): 变卦名 | 第4格(132-175): 变卦图

    // 本卦名（第1格，可点击）
    bengua_name_label_ = lv_label_create(result_hex_area_);
    lv_obj_set_style_text_font(bengua_name_label_, text_font_, 0);
    lv_obj_set_pos(bengua_name_label_, 6, 8);
    lv_label_set_text(bengua_name_label_, "本卦");
    lv_obj_add_flag(bengua_name_label_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bengua_name_label_, OnBenguaClick, LV_EVENT_CLICKED, this);

    // 变卦名（第3格，可点击）
    biangua_name_label_ = lv_label_create(result_hex_area_);
    lv_obj_set_style_text_font(biangua_name_label_, text_font_, 0);
    lv_obj_set_pos(biangua_name_label_, 94, 8);
    lv_label_set_text(biangua_name_label_, "变卦");
    lv_obj_add_flag(biangua_name_label_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(biangua_name_label_, OnBianguaClick, LV_EVENT_CLICKED, this);

    // 原始爻值标签（放到文本区域内显示，不在顶部区域）
    raw_yao_label_ = nullptr;  // 不再作为独立标签使用

    // 文本滚动区域: 包含原始爻值+爻辞+断语 (y: 34~228, 高度194)
    result_scroll_ = lv_obj_create(scr_result_);
    lv_obj_set_size(result_scroll_, 174, 194);
    lv_obj_align(result_scroll_, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_border_width(result_scroll_, 1, 0);
    lv_obj_set_style_border_color(result_scroll_, lv_color_black(), 0);
    lv_obj_set_style_radius(result_scroll_, 0, 0);
    lv_obj_set_style_pad_all(result_scroll_, 4, 0);
    lv_obj_set_scroll_dir(result_scroll_, LV_DIR_VER);
    lv_obj_add_flag(result_scroll_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(result_scroll_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(result_scroll_, LV_SCROLLBAR_MODE_AUTO);
    result_text_ = lv_label_create(result_scroll_);
    lv_obj_set_style_text_font(result_text_, text_font_, 0);
    lv_obj_set_width(result_text_, 164);
    lv_label_set_long_mode(result_text_, LV_LABEL_LONG_WRAP);
    lv_obj_align(result_text_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_align(result_text_, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(result_text_, "结果待生成");

    // 重新起卦按钮 (y: 230~262)
    lv_obj_t* result_btn = lv_btn_create(scr_result_);
    lv_obj_set_size(result_btn, 90, 32);
    lv_obj_align(result_btn, LV_ALIGN_TOP_MID, 0, 230);
    lv_obj_add_event_cb(result_btn, OnResultBtn, LV_EVENT_CLICKED, this);
    lv_obj_t* result_btn_label = lv_label_create(result_btn);
    lv_obj_set_style_text_font(result_btn_label, text_font_, 0);
    lv_label_set_text(result_btn_label, "重新起卦");
    lv_obj_center(result_btn_label);

    // 详情页
    scr_detail_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_detail_, text_font_, 0);
    lv_obj_set_style_bg_color(scr_detail_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_detail_, lv_color_black(), 0);
    lv_obj_clear_flag(scr_detail_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_detail_, LV_SCROLLBAR_MODE_OFF);
    // 详情页顶部：返回按钮 + 卦名
    lv_obj_t* detail_header = lv_obj_create(scr_detail_);
    lv_obj_set_size(detail_header, 172, 30);
    lv_obj_align(detail_header, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_opa(detail_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(detail_header, 0, 0);
    lv_obj_set_style_pad_all(detail_header, 0, 0);
    lv_obj_clear_flag(detail_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* back_btn = lv_btn_create(detail_header);
    lv_obj_set_size(back_btn, 50, 26);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_add_event_cb(back_btn, OnDetailBack, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_label, text_font_, 0);
    lv_label_set_text(back_label, "返回");
    lv_obj_center(back_label);
    lv_obj_t* detail_title = lv_label_create(detail_header);
    lv_obj_set_style_text_font(detail_title, text_font_, 0);
    lv_obj_align(detail_title, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(detail_title, "卦详情");
    // 详情页内容滚动区
    detail_scroll_ = lv_obj_create(scr_detail_);
    lv_obj_set_size(detail_scroll_, 172, 220);
    lv_obj_align(detail_scroll_, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_border_width(detail_scroll_, 1, 0);
    lv_obj_set_style_border_color(detail_scroll_, lv_color_black(), 0);
    lv_obj_set_style_radius(detail_scroll_, 0, 0);
    lv_obj_set_style_pad_all(detail_scroll_, 4, 0);
    lv_obj_set_scroll_dir(detail_scroll_, LV_DIR_VER);
    lv_obj_add_flag(detail_scroll_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(detail_scroll_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(detail_scroll_, LV_SCROLLBAR_MODE_AUTO);
    detail_text_ = lv_label_create(detail_scroll_);
    lv_obj_set_style_text_font(detail_text_, text_font_, 0);
    lv_obj_set_width(detail_text_, 160);
    lv_label_set_long_mode(detail_text_, LV_LABEL_LONG_WRAP);
    lv_obj_align(detail_text_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_align(detail_text_, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(detail_text_, "详情内容");

    scr_shutdown_ = lv_obj_create(nullptr);
    // 画面铺满黑色作底，整个 176x264 显示生成器导出的关机插画。
    lv_obj_set_style_bg_color(scr_shutdown_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr_shutdown_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_shutdown_, 0, 0);
    lv_obj_set_style_border_width(scr_shutdown_, 0, 0);
    lv_obj_clear_flag(scr_shutdown_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* shutdown_img = lv_image_create(scr_shutdown_);
    lv_image_set_src(shutdown_img, &shutdown_image);
    lv_obj_center(shutdown_img);

    // 系统级提示页：低电倒计时、电池过放等场景使用。
    scr_system_tip_ = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr_system_tip_, text_font_, 0);
    lv_obj_set_style_bg_color(scr_system_tip_, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr_system_tip_, lv_color_black(), 0);
    lv_obj_clear_flag(scr_system_tip_, LV_OBJ_FLAG_SCROLLABLE);
    system_tip_title_ = lv_label_create(scr_system_tip_);
    lv_obj_set_style_text_font(system_tip_title_, text_font_, 0);
    lv_label_set_long_mode(system_tip_title_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(system_tip_title_, 160);
    lv_obj_set_style_text_align(system_tip_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(system_tip_title_, "");
    lv_obj_align(system_tip_title_, LV_ALIGN_CENTER, 0, -24);
    system_tip_body_ = lv_label_create(scr_system_tip_);
    lv_obj_set_style_text_font(system_tip_body_, text_font_, 0);
    lv_label_set_long_mode(system_tip_body_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(system_tip_body_, 160);
    lv_obj_set_style_text_align(system_tip_body_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(system_tip_body_, "");
    lv_obj_align(system_tip_body_, LV_ALIGN_CENTER, 0, 24);
}

const lv_font_t* DayanUi::ResolveTextFont() {
    if (BoardStorageMounted()) {
        // 从 80M 存储加载 binfont，便于后续只换字库文件不重刷固件。
        sd_font_ = lv_binfont_create(BoardLvglFontPath());
        if (sd_font_ != nullptr) {
            // SD 字体也挂上 combined_font_ 作为 fallback，确保冷僻字有保底。
            sd_font_->fallback = &combined_font_;
            return sd_font_;
        }
    }
    // 无 SD：用 patch→puhui 合并链作为内置字体。
    return &combined_font_;
}

void DayanUi::ShowWelcome() {
    // 每次进入欢迎页前都刷新一次电量，保证显示是最新值。
    UpdateWelcomeBattery();
    lv_screen_load(scr_welcome_);
    lv_obj_update_layout(scr_welcome_);
    lv_obj_invalidate(scr_welcome_);
    if (!first_welcome_paint_done_) {
        // 仅首次首页绘制前留一点稳定时间，并使用全刷波形渲染首帧，降低“发虚”概率。
        vTaskDelay(pdMS_TO_TICKS(120));
        panel_gdey027t91_refresh_code(0xF7);
        first_welcome_paint_done_ = true;
    }
    // 启动阶段仍只做一次黑白预刷新；这里执行首页内容绘制。
    lv_refr_now(nullptr);
}

void DayanUi::UpdateWelcomeBattery() {
    if (welcome_battery_label_ == nullptr || welcome_battery_icon_ == nullptr) {
        return;
    }
    const int level = BoardGetBatteryLevelPercent();
    if (level < 0) {
        // 电量计不可用时显示降级状态，方便现场排查硬件问题。
        lv_label_set_text(welcome_battery_label_, "--%");
        lv_label_set_text(welcome_battery_icon_, FONT_AWESOME_BATTERY_SLASH);
        return;
    }
    char text[16];
    std::snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(level));
    lv_label_set_text(welcome_battery_label_, text);
    // 充电中（含预充/充电/已充满）统一显示闪电图标，便于用户一眼分辨外接电源状态。
    const int charge_state = BoardGetChargeState();
    if (charge_state > 0) {
        lv_label_set_text(welcome_battery_icon_, FONT_AWESOME_BATTERY_CHARGING);
        return;
    }
    // 按电量区间切换图标，视觉风格与 xiaozhi-card 保持一致。
    const char* icon = FONT_AWESOME_BATTERY_EMPTY;
    if (level >= 80) {
        icon = FONT_AWESOME_BATTERY_FULL;
    } else if (level >= 60) {
        icon = FONT_AWESOME_BATTERY_3;
    } else if (level >= 40) {
        icon = FONT_AWESOME_BATTERY_2;
    } else if (level >= 20) {
        icon = FONT_AWESOME_BATTERY_1;
    }
    lv_label_set_text(welcome_battery_icon_, icon);
}

void DayanUi::ShowSystemTip(const char* title, const char* body) {
    if (scr_system_tip_ == nullptr) {
        return;
    }
    lv_label_set_text(system_tip_title_, title != nullptr ? title : "");
    lv_label_set_text(system_tip_body_, body != nullptr ? body : "");
    if (lv_screen_active() != scr_system_tip_) {
        lv_screen_load(scr_system_tip_);
    }
}

void DayanUi::ShowIntro() {
    lv_screen_load(scr_intro_);
}

void DayanUi::ShowDivination() {
    lv_screen_load(scr_divination_);
    // 先完成布局再画蓍草，避免首帧出现横线/错位。
    lv_obj_update_layout(scr_divination_);
    RebuildSticks(49);
    lv_label_set_text(label_progress_, "这是第1次点击");
    lv_label_set_text(label_split_info_, "左边0根，右边49根");
}

void DayanUi::UpdateDivination(const DayanEngine& engine) {
    char text[48];
    const int clicked = engine.CurrentClickIndex();
    const int current_round = std::min(18, clicked + 1);
    std::snprintf(text, sizeof(text), "这是第%d次点击", current_round);
    lv_label_set_text(label_progress_, text);
    RebuildSticks(engine.CurrentTotalSticks());
}

void DayanUi::UpdateSplitInfo(int left_count, int right_count) {
    char text[64];
    std::snprintf(text, sizeof(text), "左边%d根，右边%d根", left_count, right_count);
    lv_label_set_text(label_split_info_, text);
}

void DayanUi::ShowResult(const GuaDetails& details, const DayanEngine& engine) {
    lv_screen_load(scr_result_);
    // 清除上次的卦象图对象
    for (lv_obj_t* obj : hexagram_objs_) {
        if (obj != nullptr) {
            lv_obj_del(obj);
        }
    }
    hexagram_objs_.clear();
    // 获取爻值数组
    int yao_values[6] = {0};
    engine.GetYaoValues(yao_values);
    memcpy(current_yao_values_, yao_values, sizeof(current_yao_values_));
    // 绘制本卦（第2格居中）：6/9互换前的原始卦象
    // 第2格中心x=66，卦图宽16，左边缘=66-8=58
    DrawHexagram(result_hex_area_, yao_values, 58, 2, 16);
    // 绘制变卦（第4格居中）：6/9互换
    int changed_values[6];
    for (int i = 0; i < 6; ++i) {
        if (yao_values[i] == 6) changed_values[i] = 7;
        else if (yao_values[i] == 9) changed_values[i] = 8;
        else changed_values[i] = yao_values[i];
    }
    memcpy(current_changed_yao_values_, changed_values, sizeof(current_changed_yao_values_));
    // 第4格中心x=154，卦图宽16，左边缘=154-8=146
    DrawHexagram(result_hex_area_, changed_values, 146, 2, 16);
    // 更新卦名
    lv_label_set_text(bengua_name_label_, details.getgua.c_str());
    lv_label_set_text(biangua_name_label_, details.g_gua.c_str());
    // 存储卦象信息用于详情页
    current_bengua_name_ = details.getgua;
    current_biangua_name_ = details.g_gua;
    current_bengua_content_ = details.yao_results;
    current_biangua_content_ = details.yao_results;
    // 结果页内容：原始爻值+本卦爻辞+变卦爻辞+动爻信息
    char yao_str[32];
    std::snprintf(yao_str, sizeof(yao_str), "%d%d%d%d%d%d",
                  yao_values[5], yao_values[4], yao_values[3],
                  yao_values[2], yao_values[1], yao_values[0]);
    std::string out;
    out += "原始爻值: " + std::string(yao_str) + "\n\n";
    out += "【本卦】" + details.getgua + "\n";
    out += details.yao_results + "\n\n";
    out += "【变卦】" + details.g_gua + "\n";
    out += details.changed_yao + "\n\n";
    out += details.yao_text;
    lv_label_set_text(result_text_, out.c_str());
    lv_obj_scroll_to_y(result_scroll_, 0, LV_ANIM_OFF);
}

void DayanUi::ShowShutdown() {
    lv_screen_load(scr_shutdown_);
}

void DayanUi::DrawHexagram(lv_obj_t* parent, const int yao_values[6], int x, int y, int width) {
    // 紧凑卦象图：整体约 28x30 像素
    constexpr int kLineHeight = 3;   // 爻线高度
    constexpr int kLineGap = 1;      // 爻间距
    constexpr int kYinGap = 4;       // 阴爻中间空隙
    const int total_height = 6 * kLineHeight + 5 * kLineGap;
    // 从下到上绘制：index 0=初爻(bottom)，5=上爻(top)
    for (int i = 0; i < 6; ++i) {
        const int yao = yao_values[i];
        const int line_y = y + total_height - (i + 1) * (kLineHeight + kLineGap) + kLineGap;
        const bool is_yang = (yao == 7 || yao == 9);
        const bool is_moving = (yao == 6 || yao == 9);
        if (is_yang) {
            // 阳爻：一条实线（用细矩形模拟）
            lv_obj_t* line = lv_obj_create(parent);
            lv_obj_set_size(line, width, kLineHeight);
            lv_obj_set_pos(line, x, line_y);
            lv_obj_set_style_bg_color(line, lv_color_black(), 0);
            lv_obj_set_style_border_width(line, 0, 0);
            lv_obj_set_style_radius(line, 0, 0);
            lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
            hexagram_objs_.push_back(line);
        } else {
            // 阴爻：两段线，中间留空
            const int seg_width = (width - kYinGap) / 2;
            lv_obj_t* left = lv_obj_create(parent);
            lv_obj_set_size(left, seg_width, kLineHeight);
            lv_obj_set_pos(left, x, line_y);
            lv_obj_set_style_bg_color(left, lv_color_black(), 0);
            lv_obj_set_style_border_width(left, 0, 0);
            lv_obj_set_style_radius(left, 0, 0);
            lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
            hexagram_objs_.push_back(left);
            lv_obj_t* right = lv_obj_create(parent);
            lv_obj_set_size(right, seg_width, kLineHeight);
            lv_obj_set_pos(right, x + seg_width + kYinGap, line_y);
            lv_obj_set_style_bg_color(right, lv_color_black(), 0);
            lv_obj_set_style_border_width(right, 0, 0);
            lv_obj_set_style_radius(right, 0, 0);
            lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);
            hexagram_objs_.push_back(right);
        }
        if (is_moving) {
            // 动爻标记：小圆点（2x2）
            lv_obj_t* dot = lv_obj_create(parent);
            lv_obj_set_size(dot, 3, 3);
            lv_obj_set_pos(dot, x + width + 1, line_y + (kLineHeight - 3) / 2);
            lv_obj_set_style_bg_color(dot, lv_color_black(), 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            hexagram_objs_.push_back(dot);
        }
    }
}

namespace {
void OnChargingTipTimer(lv_timer_t* timer) {
    auto* box = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
    if (box != nullptr) {
        lv_obj_del(box);
    }
    lv_timer_delete(timer);
}
}  // namespace

void DayanUi::ShowChargingNoShutdownTip() {
    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(box, 168, 72);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_black(), 0);
    lv_obj_set_style_text_font(box, text_font_, 0);
    lv_obj_set_style_text_color(box, lv_color_black(), 0);
    lv_obj_t* t1 = lv_label_create(box);
    lv_obj_set_style_text_font(t1, text_font_, 0);
    lv_label_set_text(t1, "充电中");
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_t* t2 = lv_label_create(box);
    lv_obj_set_style_text_font(t2, text_font_, 0);
    lv_label_set_text(t2, "不能关机");
    lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 32);
    lv_refr_now(nullptr);
    lv_timer_t* timer = lv_timer_create(OnChargingTipTimer, 2000, box);
    lv_timer_set_repeat_count(timer, 1);
}

bool DayanUi::ResolveGapClick(int x, int y, int stick_count, int& out_left_count) const {
    // 只有点在“蓍草缝隙”才算有效点击；点在草身或外部均忽略。
    lv_area_t box;
    lv_obj_get_coords(frame_box_, &box);
    if (x <= box.x1 || x >= box.x2 || y <= box.y1 || y >= box.y2) {
        return false;
    }
    const int inner_left = box.x1 + kStickSidePadding;
    const int inner_right = box.x2 - kStickSidePadding;
    const int usable_w = std::max(1, inner_right - inner_left + 1);
    const int stick_w = 1;
    const int count = std::max(stick_count, 1);
    int gap = 1;
    if (count > 1) {
        gap = std::max(1, (usable_w - count * stick_w) / (count - 1));
    }
    const int used_w = count * stick_w + (count - 1) * gap;
    const int start_x = inner_left + std::max(0, (usable_w - used_w) / 2);

    for (int i = 0; i < stick_count - 1; ++i) {
        const int sx = start_x + i * (stick_w + gap);
        const int gap_start = sx + stick_w;
        const int gap_end = sx + stick_w + gap;
        // Keep hit ranges non-overlapping; otherwise touches bias to small i.
        const int tolerance = (gap >= 3) ? 1 : 0;
        const int hit_start = gap_start - tolerance;
        const int hit_end = gap_end + tolerance;
        if (x >= hit_start && x <= hit_end) {
            out_left_count = i + 1;
            return true;
        }
    }
    return false;
}

void DayanUi::RebuildSticks(int stick_count) {
    EnsureStickPool();

    const int layer_w = lv_obj_get_width(stick_layer_);
    const int layer_h = lv_obj_get_height(stick_layer_);
    const int stick_w = 1;
    const int count = std::clamp(stick_count, 1, kMaxSticks);
    const int usable_w = std::max(1, layer_w - 2 * kStickSidePadding);
    int gap = 1;
    if (count > 1) {
        gap = std::max(1, (usable_w - count * stick_w) / (count - 1));
    }
    const int used_w = count * stick_w + (count - 1) * gap;
    const int start_x = kStickSidePadding + std::max(0, (usable_w - used_w) / 2);
    const int stick_h = std::max(1, layer_h - 2 * kStickTopPadding);
    for (int i = 0; i < kMaxSticks; ++i) {
        lv_obj_t* stick = stick_objs_[i];
        if (i < count) {
            lv_obj_set_size(stick, stick_w, stick_h);
            lv_obj_set_pos(stick, start_x + i * (stick_w + gap), kStickTopPadding);
            lv_obj_clear_flag(stick, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(stick, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void DayanUi::EnsureStickPool() {
    if (static_cast<int>(stick_objs_.size()) == kMaxSticks) {
        return;
    }
    for (lv_obj_t* obj : stick_objs_) {
        lv_obj_delete(obj);
    }
    stick_objs_.clear();
    stick_objs_.reserve(kMaxSticks);
    for (int i = 0; i < kMaxSticks; ++i) {
        lv_obj_t* stick = lv_obj_create(stick_layer_);
        lv_obj_set_size(stick, 1, 1);
        lv_obj_set_style_bg_color(stick, lv_color_black(), 0);
        lv_obj_set_style_border_width(stick, 0, 0);
        lv_obj_set_style_radius(stick, 0, 0);
        lv_obj_add_flag(stick, LV_OBJ_FLAG_HIDDEN);
        stick_objs_.push_back(stick);
    }
}

void DayanUi::OnWelcomeBtn(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self->on_welcome_confirm) {
        self->on_welcome_confirm();
    }
}

void DayanUi::OnIntroBtn(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self->on_intro_start) {
        self->on_intro_start();
    }
}

void DayanUi::OnDivinationTouch(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (!self->on_divination_click) {
        return;
    }
    lv_indev_t* indev = lv_event_get_indev(e);
    if (indev == nullptr) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    self->on_divination_click(p.x, p.y);
}

void DayanUi::OnResultBtn(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self->on_result_restart) {
        self->on_result_restart();
    }
}

void DayanUi::OnBenguaClick(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self != nullptr) {
        self->ShowGuaDetail(self->current_bengua_name_.c_str(), GetGuaDetailTextByName(self->current_bengua_name_));
    }
}

void DayanUi::OnBianguaClick(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self != nullptr) {
        self->ShowGuaDetail(self->current_biangua_name_.c_str(), GetGuaDetailTextByName(self->current_biangua_name_));
    }
}

void DayanUi::OnDetailBack(lv_event_t* e) {
    auto* self = static_cast<DayanUi*>(lv_event_get_user_data(e));
    if (self != nullptr && self->on_detail_back) {
        self->on_detail_back();
    }
}

void DayanUi::ShowGuaDetail(const char* gua_name, const std::string& gua_content) {
    lv_screen_load(scr_detail_);
    // 更新标题
    lv_obj_t* header = lv_obj_get_child(scr_detail_, 0);
    if (header != nullptr) {
        lv_obj_t* title = lv_obj_get_child(header, 2);
        if (title != nullptr) {
            lv_label_set_text(title, gua_name);
        }
    }
    // 更新内容
    lv_label_set_text(detail_text_, gua_content.c_str());
    lv_obj_scroll_to_y(detail_scroll_, 0, LV_ANIM_OFF);
}
