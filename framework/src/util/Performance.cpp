//
// Created by Zach Lee on 2022/8/20.
//

#include <SDL2/SDL.h>
#include <framework/util/Performance.h>

namespace sky {

    uint64_t GetPerformanceFrequency()
    {
        return SDL_GetPerformanceFrequency();
    }

    uint64_t GetPerformanceCounter()
    {
        return SDL_GetPerformanceCounter();
    }

} // namespace sky