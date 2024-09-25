//
// Created by Zach Lee on 2023/4/2.
//

#include <core/util/String.h>
#include <codecvt>

namespace sky {

    const std::string &GetEmpty()
    {
        static std::string empty;
        return empty;
    }

    std::vector<std::string> Split(const std::string &s, const char *separator)
    {
        std::vector<std::string> output;
        std::string::size_type prev = 0, pos = 0;
        while ((pos = s.find_first_of(std::string(separator), pos)) != std::string::npos) {
            if (pos > prev) {
                std::string substring(s.substr(prev, pos - prev));
                output.push_back(substring);
            }
            prev = ++pos;
        }

        if (prev <= s.length()) {
            output.push_back(s.substr(prev, pos - prev));
        }
        return output;
    }


    std::wstring Utf8ToUtf16(const std::string &str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(str);
    }
}