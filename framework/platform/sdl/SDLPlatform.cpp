//
// Created by Zach Lee on 2022/9/25.
//

#include "SDLPlatform.h"
#include <core/logger/Logger.h>
#include <SDL2/SDL.h>

static const char* TAG = "SDLPlatform";

namespace sky {

    bool SDLPlatform::Init()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOG_E(TAG, "SDL could not be initialized! Error: %s", SDL_GetError());
            return false;
        }
        return true;
    }

    void SDLPlatform::Shutdown()
    {
        SDL_Quit();
    }

    uint64_t SDLPlatform::GetPerformanceFrequency() const
    {
        return SDL_GetPerformanceFrequency();
    }

    uint64_t SDLPlatform::GetPerformanceCounter() const
    {
        return SDL_GetPerformanceCounter();
    }
}