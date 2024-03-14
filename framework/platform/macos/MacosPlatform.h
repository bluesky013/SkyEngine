//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../genetic/SDLPlatform.h"

namespace sky {

    class MacosPlatform : public SDLPlatform {
    public:
        MacosPlatform() = default;
        ~MacosPlatform() = default;

        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;

        PlatformType GetType() const override { return PlatformType::MacOS; }
    };
}
