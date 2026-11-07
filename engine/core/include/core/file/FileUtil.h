//
// Created by Zach Lee on 2022/3/11.
//

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sky {

    bool ConstructFullPath(const std::string &root, const std::string &relative, std::string &out);

    // Builds a filesystem path from UTF-8 text (std::filesystem::u8path is deprecated in C++20).
    inline std::filesystem::path PathFromUtf8(std::string_view value)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
    }
} // namespace sky