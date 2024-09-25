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

    std::wstring Utf8ToUtf16(const std::string &path);
}