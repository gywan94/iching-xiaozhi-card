#pragma once

#include <string>

struct GuaDetails {
    std::string guayao;
    std::string getgua;
    std::string g_gua;
    std::string yao_results;
    std::string explaination2;
    std::string origin_name;
    std::string changed_name;
    std::string yao_text;
    std::string changed_yao;
};

bool GetBookGuaDetails(const std::string& raw_line_code, GuaDetails& out);
