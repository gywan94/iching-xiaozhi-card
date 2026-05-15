#include "dayan_engine.h"

#include <array>

void DayanEngine::Reset() {
    state_ = 1;
    qian_zong_ = 49;
    dayan_ = 49;
    for (int& value : yijing_result_) {
        value = 0;
    }
    line_code_ = "000000";
    changed_line_code_ = "000000";
}

int DayanEngine::Guiji(int qian_zong, int qianshu) {
    int yu_left = qianshu % 4;
    int yu_right = (qian_zong - 1 - qianshu) % 4;
    if (yu_left == 0) {
        yu_left = 4;
    }
    if (yu_right == 0) {
        yu_right = 4;
    }
    return yu_left + yu_right + 1;
}

SplitResult DayanEngine::AdvanceWithLeftCount(int left_count) {
    SplitResult result;
    if (state_ > 18) {
        return result;
    }

    const int before_qian_zong = qian_zong_;
    const int max_left = before_qian_zong - 1;
    if (max_left <= 0) {
        return result;
    }
    if (left_count < 1) {
        left_count = 1;
    } else if (left_count > max_left) {
        left_count = max_left;
    }

    const int right_count = before_qian_zong - left_count;
    const int remove = Guiji(before_qian_zong, left_count);
    qian_zong_ = before_qian_zong - remove;
    dayan_ = qian_zong_ + 1;

    result.accepted = true;
    result.total_sticks = dayan_;
    result.left = left_count;
    result.right = right_count;
    result.click_index = state_;
    result.line_index = (state_ - 1) / 3 + 1;
    result.change_index = (state_ - 1) % 3 + 1;

    if (state_ % 3 == 0) {
        const int line_idx = state_ / 3 - 1;
        yijing_result_[line_idx] = qian_zong_ / 4;
        qian_zong_ = 49;
        dayan_ = 49;
        UpdateCodes();
    }
    state_ += 1;
    return result;
}

void DayanEngine::UpdateCodes() {
    std::array<char, 7> origin = {'0', '0', '0', '0', '0', '0', '\0'};
    std::array<char, 7> changed = {'0', '0', '0', '0', '0', '0', '\0'};
    for (int i = 0; i < 6; ++i) {
        const int value = yijing_result_[i];
        char line = '0';
        char changed_line = '0';
        if (value == 6) {
            line = '0';
            changed_line = '1';
        } else if (value == 7) {
            line = '1';
            changed_line = '1';
        } else if (value == 8) {
            line = '0';
            changed_line = '0';
        } else if (value == 9) {
            line = '1';
            changed_line = '0';
        }
        origin[i] = line;
        changed[i] = changed_line;
    }
    line_code_ = origin.data();
    changed_line_code_ = changed.data();
}

std::string DayanEngine::GetLineCode() const {
    return line_code_;
}

std::string DayanEngine::GetChangedLineCode() const {
    return changed_line_code_;
}

std::string DayanEngine::GetRawLineCode() const {
    std::string raw;
    raw.reserve(6);
    for (int i = 0; i < 6; ++i) {
        const int value = yijing_result_[i];
        if (value >= 6 && value <= 9) {
            raw.push_back(static_cast<char>('0' + value));
        } else {
            raw.push_back('8');
        }
    }
    return raw;
}

void DayanEngine::GetYaoValues(int out_values[6]) const {
    for (int i = 0; i < 6; ++i) {
        out_values[i] = yijing_result_[i];
    }
}
