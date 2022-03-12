//
// Created by Zach Lee on 2022/3/11.
//

#include <core/file/FileUtil.h>
#include <filesystem>

namespace sky {

    bool ConstructFullPath(const std::string& root, const std::string& relative, std::string& out)
    {
        std::filesystem::path fsRelative(relative);
        std::filesystem::path fsAbsolute(root);
        if (!fsRelative.is_relative() || !fsAbsolute.is_absolute()) {
            return false;
        }
        fsAbsolute /= fsRelative;
        out = fsAbsolute.generic_string();
        return true;
    }

}