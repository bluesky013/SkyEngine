//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../genetic/SDLPlatform.h"

namespace sky {

    class Win32Platform : public SDLPlatform {
    public:
        Win32Platform() = default;
        ~Win32Platform() = default;

        bool RunCmd(const std::string &str, std::string &out) const;
    };
}