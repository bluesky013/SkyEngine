//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <vector>
#include <string>

namespace sky {
    namespace String {
        const std::string &GetEmpty();
    }

    std::vector<std::string> Split(const std::string& s, const char *separator);

    std::wstring Utf8ToUtf16(const std::string &str);
    std::u16string Utf8ToUtf16U(const std::string &str);
    std::string Utf16ToUtf8(const std::wstring &str);
    std::string Utf16ToUtf8(const std::u16string &str);
}