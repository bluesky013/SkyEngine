//
// Created by blues on 2024/1/1.
//

#pragma once

#include <framework/application/Application.h>

namespace sky {
#ifdef SKY_EDITOR

    class ToolApplicationBase : public Application {
    public:
        ToolApplicationBase() = default;
        ~ToolApplicationBase() override = default;

        void LoadConfigs() override;
        void ParseStartArgs() override;
        void PostInit() override;

    protected:
        std::string projectPath;
    };

#endif
} // namespace sky