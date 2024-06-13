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

        char* GetClipBoardText() override;
        void FreeClipBoardText(char* text) override;
        void SetClipBoardText(const std::string &text) override;
    };
} // namespace sky
