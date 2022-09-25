//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../sdl/SDLPlatform.h"

namespace sky {

    class Win32Platform : public SDLPlatform {
    public:
        Win32Platform() = default;
        ~Win32Platform() = default;
    };
}