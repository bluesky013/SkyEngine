//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <cstdint>
#include <framework/platform/PlatformBase.h>

namespace sky {

    class SDLPlatform : public PlatformBase {
    public:
        SDLPlatform() = default;
        ~SDLPlatform() override;

    private:
        bool Init(const PlatformInfo &desc) override;

        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;
        std::string GetInternalPath() const override { return ""; }

        std::string GetBundlePath() const;
        std::string GetWritablePath() const;
    };
}
