#pragma once

#include <cstdint>
#include <string>

struct SplitResult {
    bool accepted = false;
    int total_sticks = 0;
    int left = 0;
    int right = 0;
    int click_index = 0;
    int line_index = 0;
    int change_index = 0;
};

class DayanEngine {
public:
    void Reset();
    SplitResult AdvanceWithLeftCount(int left_count);
    bool IsFinished() const { return state_ > 18; }
    int CurrentTotalSticks() const { return dayan_; }
    int CurrentClickIndex() const { return state_ - 1; }
    std::string GetLineCode() const;
    std::string GetChangedLineCode() const;
    std::string GetRawLineCode() const;

private:
    static int Guiji(int qian_zong, int qianshu);
    void UpdateCodes();

    int state_ = 1;
    int qian_zong_ = 49;
    int dayan_ = 49;
    int yijing_result_[6] = {0, 0, 0, 0, 0, 0};
    std::string line_code_ = "000000";
    std::string changed_line_code_ = "000000";
};
