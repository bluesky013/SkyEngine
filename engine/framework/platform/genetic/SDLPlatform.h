//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <cstdint>
#include <framework/platform/PlatformBase.h>

union SDL_Event;

namespace sky {

    class SDLPlatform : public PlatformBase {
    public:
        SDLPlatform() = default;
        ~SDLPlatform() override;

    private:
        bool Init(const PlatformInfo &desc) override;

        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;

        std::string GetUserConfigPath() const override;

        void PollEvent(bool &exit) override;
        void Dispatch(const SDL_Event &sdlEvent, bool &quit);
        void DispatchWindowEvent(const SDL_Event &sdlEvent);

        char* GetClipBoardText() override;
        void FreeClipBoardText(char* text) override;
        void SetClipBoardText(const std::string &text) override;

        void StartTextInput() override;
        void StopTextInput() override;
        void SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height) override;
    };
} // namespace sky
