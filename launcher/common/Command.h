//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace sky {

    struct CommandInfo {
        std::vector<std::string> modules;
        std::unordered_map<std::string, std::string> values;
    };

    void ProcessCommand(int argc, char **argv, CommandInfo &output);
} // namespace sky
