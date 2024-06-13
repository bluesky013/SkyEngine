//
// Created by Zach Lee on 2022/9/25.
//

#include "SDLPlatform.h"
#include <core/logger/Logger.h>
#include <SDL2/SDL.h>

static const char* TAG = "SDLPlatform";

namespace sky {

    SDLPlatform::~SDLPlatform()
    {
        SDL_Quit();
    }

    bool SDLPlatform::Init(const PlatformInfo &desc)
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOG_E(TAG, "SDL could not be initialized! Error: %s", SDL_GetError());
            return false;
        }
        return true;
    }

    uint64_t SDLPlatform::GetPerformanceFrequency() const
    {
        return SDL_GetPerformanceFrequency();
    }

    uint64_t SDLPlatform::GetPerformanceCounter() const
    {
        return SDL_GetPerformanceCounter();
    }

    char* SDLPlatform::GetClipBoardText()
    {
        return SDL_GetClipboardText();
    }

    void SDLPlatform::FreeClipBoardText(char* text)
    {
        SDL_free(text);
    }

    void SDLPlatform::SetClipBoardText(const std::string &text)
    {
        SDL_SetClipboardText(text.data());
    }

//    std::string SDLPlatform::GetBundlePath() const
//    {
//        return SDL_GetBasePath();
//    }
} // namespace sky