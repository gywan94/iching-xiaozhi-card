#include "dayan_data.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "cJSON.h"

namespace {
struct Entry {
    const char* name;
    const char* text;
};

#include "generated/iching_data.inc"

struct ParsedEntry {
    std::string guayao;
    std::unordered_map<int, std::string> yao;
    std::string yao_json;
};

int RightMostIndex(const std::string& s, char ch) {
    const size_t pos = s.rfind(ch);
    if (pos == std::string::npos) {
        return 0;
    }
    return static_cast<int>(pos) + 1;
}

int SecondRightMostIndex(const std::string& s, char ch) {
    const size_t top = s.rfind(ch);
    if (top == std::string::npos || top == 0) {
        return 0;
    }
    const size_t second = s.rfind(ch, top - 1);
    if (second == std::string::npos) {
        return 0;
    }
    return static_cast<int>(second) + 1;
}

std::string ToBinaryCode(const std::string& raw_code) {
    std::string out;
    out.reserve(raw_code.size());
    for (char ch : raw_code) {
        if (ch == '6' || ch == '8') {
            out.push_back('0');
        } else if (ch == '7' || ch == '9') {
            out.push_back('1');
        } else if (ch == '0' || ch == '1') {
            out.push_back(ch);
        }
    }
    return out;
}

std::string ToChangedStaticCode(const std::string& raw_code) {
    std::string out = raw_code;
    for (char& ch : out) {
        if (ch == '6') {
            ch = '7';
        } else if (ch == '9') {
            ch = '8';
        }
    }
    return out;
}

std::string BuildMovingMask(const std::string& raw_code) {
    std::string mask;
    mask.reserve(raw_code.size());
    for (char ch : raw_code) {
        if (ch == '6' || ch == '9') {
            mask.push_back('1');
        } else {
            mask.push_back('0');
        }
    }
    return mask;
}

std::string PrefixBeforeColon(const std::string& text) {
    size_t pos = text.find("：");
    if (pos == std::string::npos) {
        pos = text.find(':');
    }
    if (pos == std::string::npos) {
        return text;
    }
    return text.substr(0, pos);
}

bool ParseEntryText(const char* text, ParsedEntry& out) {
    cJSON* root = cJSON_Parse(text);
    if (root == nullptr || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON* guayao = cJSON_GetArrayItem(root, 0);
    cJSON* yao_obj = cJSON_GetArrayItem(root, 3);
    if (!cJSON_IsString(guayao) || !cJSON_IsObject(yao_obj)) {
        cJSON_Delete(root);
        return false;
    }
    out.guayao = guayao->valuestring;
    out.yao_json.clear();
    out.yao.clear();
    cJSON* child = yao_obj->child;
    while (child != nullptr) {
        if (child->string != nullptr && cJSON_IsString(child)) {
            int key = 0;
            bool valid = true;
            for (const char* p = child->string; *p != '\0'; ++p) {
                if (!std::isdigit(static_cast<unsigned char>(*p))) {
                    valid = false;
                    break;
                }
                key = key * 10 + (*p - '0');
            }
            if (valid) {
                out.yao[key] = child->valuestring;
            }
        }
        child = child->next;
    }
    char* printed = cJSON_PrintUnformatted(yao_obj);
    if (printed != nullptr) {
        out.yao_json = printed;
        cJSON_free(printed);
    }
    cJSON_Delete(root);
    return true;
}

std::string GetYaoText(const ParsedEntry& entry, int idx) {
    const auto it = entry.yao.find(idx);
    if (it == entry.yao.end()) {
        return {};
    }
    return it->second;
}

std::string RemovePrefixTwoChars(const std::string& text) {
    // 对齐 Python [2:] 语义，按 UTF-8 字符边界跳过前两个字符（即“彖：”，每字3字节共 6 字节）。
    // substr(2) 只跳 2 字节会截断多字节汉字导致乱码，必须按字符边界跳。
    size_t i = 0;
    int skipped = 0;
    while (i < text.size() && skipped < 2) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { i += 1; }
        else if (c < 0xE0) { i += 2; }
        else if (c < 0xF0) { i += 3; }
        else { i += 4; }
        ++skipped;
    }
    return (i < text.size()) ? text.substr(i) : std::string{};
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        if (!first) {
            oss << "\n";
        }
        first = false;
        oss << line;
    }
    return oss.str();
}

std::string ToJsonArrayString(const std::vector<std::string>& lines) {
    cJSON* arr = cJSON_CreateArray();
    for (const auto& line : lines) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(line.c_str()));
    }
    char* printed = cJSON_PrintUnformatted(arr);
    std::string out;
    if (printed != nullptr) {
        out = printed;
        cJSON_free(printed);
    }
    cJSON_Delete(arr);
    return out;
}
}  // namespace

std::string GetGuaYaoCiByName(const std::string& gua_name) {
    // 遍历查找匹配的卦名
    for (const auto& [bin_key, entry] : kIChingData) {
        if (gua_name == entry.name) {
            ParsedEntry parsed;
            if (!ParseEntryText(entry.text, parsed)) {
                return "数据解析失败";
            }
            std::ostringstream oss;
            // 卦辞 (key 0)
            auto it_gua_ci = parsed.yao.find(0);
            if (it_gua_ci != parsed.yao.end()) {
                oss << "【卦辞】\n" << it_gua_ci->second << "\n\n";
            }
            // 六爻爻辞 (key 1-6)
            const char* yao_names[] = {"", "初爻", "二爻", "三爻", "四爻", "五爻", "上爻"};
            for (int i = 1; i <= 6; ++i) {
                auto it = parsed.yao.find(i);
                if (it != parsed.yao.end()) {
                    oss << "【" << yao_names[i] << "】\n" << it->second << "\n\n";
                }
            }
            // 彖辞 (key 7)
            auto it_tuan = parsed.yao.find(7);
            if (it_tuan != parsed.yao.end()) {
                oss << "【彖辞】\n" << it_tuan->second;
            }
            return oss.str();
        }
    }
    return "未找到卦象数据";
}

// 包含从HTML提取的卦详细文本数据
#include "generated/gua_detail_text.inc"

std::string GetGuaDetailTextByName(const std::string& gua_name) {
    const auto it = kGuaDetailText.find(gua_name);
    if (it != kGuaDetailText.end()) {
        return it->second;
    }
    return "未找到卦象详细解读";
}

bool GetBookGuaDetails(const std::string& raw_line_code, GuaDetails& out) {
    const std::string origin_code = ToBinaryCode(raw_line_code);
    const std::string changed_static_code = ToChangedStaticCode(raw_line_code);
    const std::string changed_code = ToBinaryCode(changed_static_code);
    if (origin_code.size() != 6 || changed_code.size() != 6) {
        return false;
    }

    const auto it_origin = kIChingData.find(origin_code);
    if (it_origin == kIChingData.end()) {
        return false;
    }
    const auto it_changed = kIChingData.find(changed_code);
    if (it_changed == kIChingData.end()) {
        return false;
    }

    ParsedEntry origin_entry;
    ParsedEntry changed_entry;
    if (!ParseEntryText(it_origin->second.text, origin_entry) || !ParseEntryText(it_changed->second.text, changed_entry)) {
        return false;
    }

    out.origin_name = it_origin->second.name;
    out.changed_name = it_changed->second.name;
    const std::string moving_mask = BuildMovingMask(raw_line_code);
    const int moving_count = static_cast<int>(std::count(moving_mask.begin(), moving_mask.end(), '1'));
    const std::string explain = "动爻有【" + std::to_string(moving_count) + "】根。";
    const std::string gua_pair = "【" + out.origin_name + "之" + out.changed_name + "】";
    const int top_moving = RightMostIndex(moving_mask, '1');
    const int second_moving = SecondRightMostIndex(moving_mask, '1');
    const int top_static = RightMostIndex(moving_mask, '0');
    const int second_static = SecondRightMostIndex(moving_mask, '0');
    const std::string top_text = GetYaoText(origin_entry, top_moving);
    const std::string second_text = GetYaoText(origin_entry, second_moving);
    const std::string top_static_text = GetYaoText(changed_entry, top_static);
    const std::string second_static_text = GetYaoText(changed_entry, second_static);
    const std::string origin_tuan = RemovePrefixTwoChars(GetYaoText(origin_entry, 7));
    const std::string changed_tuan = RemovePrefixTwoChars(GetYaoText(changed_entry, 7));

    std::vector<std::string> lines;
    if (moving_count == 0) {
        lines = {explain, "主要看【" + out.origin_name + "】卦彖辞。", origin_tuan};
    } else if (moving_count == 1) {
        lines = {explain, gua_pair, "主要看【" + PrefixBeforeColon(top_text) + "】", top_text};
    } else if (moving_count == 2) {
        lines = {gua_pair, explain, "主要看【" + PrefixBeforeColon(top_text) + "】，其次看【" + PrefixBeforeColon(second_text) + "】。", top_text, second_text};
    } else if (moving_count == 3) {
        const size_t first_moving = moving_mask.find('1');
        if (first_moving == 0) {
            lines = {gua_pair, explain, "【" + out.origin_name + "】卦为贞(我方)，【" + out.changed_name + "】卦为悔(他方)。前十卦，主贞【" + out.origin_name + "】卦，请参考两卦彖辞", origin_tuan, changed_tuan};
        } else {
            lines = {gua_pair, explain, "【" + out.origin_name + "】卦为贞(我方)，【" + out.changed_name + "】卦为悔(他方)。后十卦，主悔【" + out.changed_name + "】卦，请参考两卦彖辞", changed_tuan, origin_tuan};
        }
    } else if (moving_count == 4) {
        lines = {gua_pair, explain, "主要看【" + out.changed_name + "】的" + PrefixBeforeColon(second_static_text) + "，其次看" + PrefixBeforeColon(top_static_text) + "。", second_static_text, top_static_text};
    } else if (moving_count == 5) {
        lines = {gua_pair, explain, "主要看【" + out.changed_name + "】的" + PrefixBeforeColon(top_static_text) + "。", top_static_text};
    } else {
        lines = {gua_pair, explain, "主要看【" + out.changed_name + "】卦的彖辞。", changed_tuan};
    }
    out.yao_text = JoinLines(lines);
    // 变卦爻辞：使用变卦的JSON格式爻辞
    out.changed_yao = changed_entry.yao_json;
    out.guayao = raw_line_code;
    out.getgua = out.origin_name;
    out.g_gua = out.changed_name;
    out.yao_results = origin_entry.yao_json;
    out.explaination2 = ToJsonArrayString(lines);
    return true;
}
