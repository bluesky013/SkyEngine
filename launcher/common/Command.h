//
// Created by Zach Lee on 2022/6/16.
//


#pragma once

#include <vector>
#include <string>

namespace sky {

    struct CommandInfo {
        std::vector<std::string> modules;
    };

    void ProcessCommand(int argc, char** argv, CommandInfo& output);
}