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

// 通过卦名获取该卦的所有爻辞文本
// 返回格式：卦辞\n初爻\n二爻\n...\n上爻\n彖辞
std::string GetGuaYaoCiByName(const std::string& gua_name);

// 通过卦名获取该卦的详细解读文本（从HTML提取的纯文本）
// 返回完整的卦象详解，包含卦辞、爻辞、象传等
std::string GetGuaDetailTextByName(const std::string& gua_name);
