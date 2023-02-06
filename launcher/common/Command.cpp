//
// Created by Zach Lee on 2022/6/16.
//

#include "Command.h"
#include <iostream>
#include <string>
#include <framework/application/SettingRegistry.h>

namespace sky {

    void ProcessCommand(int argc, char **argv, CommandInfo &output)
    {
        for (int i = 1; i < argc; ++i) {
            auto str = argv[i];
            if (std::string(str) == "--module") {
                i++;
                if (i < argc) {
                    auto str = argv[i];
                    output.modules.emplace_back(str);
                }
            }

            if (std::string(str) == "--rhi") {
                i++;
                if (i < argc) {
                    output.values.emplace("rhi", argv[i]);
                }
            }
        }
    }

} // namespace sky
