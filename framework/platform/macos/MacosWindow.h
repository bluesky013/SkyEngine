//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../genetic/SDLWindow.h"

namespace sky {

    class MacosWindow : public SDLWindow {
    public:
        MacosWindow() = default;
        ~MacosWindow() = default;

    private:
        bool Init(const Descriptor &desc) override;
    };


}