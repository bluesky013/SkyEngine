//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include "../genetic/SDLWindow.h"

namespace sky {

    class Win32Window : public SDLWindow {
    public:
        Win32Window() = default;
        ~Win32Window() override = default;

    private:
        bool Init(const Descriptor &desc) override;
        void UpdateWindow() override;
    };


}
