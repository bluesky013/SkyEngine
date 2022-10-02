//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../sdl/SDLPlatform.h"

namespace sky {

    class MacosPlatform : public SDLPlatform {
    public:
        MacosPlatform() = default;
        ~MacosPlatform() = default;
    };
}
