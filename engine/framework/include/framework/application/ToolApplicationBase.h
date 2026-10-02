//
// Created by blues on 2024/1/1.
//

#pragma once

#include <framework/application/Application.h>

namespace sky {
    class ToolApplicationBase : public Application {
    public:
        ToolApplicationBase() = default;
        ~ToolApplicationBase() override = default;

        bool LoadConfigs() override;
        void ParseStartArgs() override;
        void PostInit() override;

    protected:
        std::string projectPath;
    };
} // namespace sky