//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../genetic/SDLPlatform.h"

namespace sky {

    class Win32Platform : public SDLPlatform {
    public:
        Win32Platform() = default;
        ~Win32Platform() override = default;

    private:
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        std::string GetEnvVariable(const std::string &env) const;
        bool RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override;

        bool ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;
        bool ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;
    };
}