//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include "framework/window/NativeWindow.h"

namespace sky {

    class AndroidWindow : public NativeWindow {
    public:
        AndroidWindow() = default;
        ~AndroidWindow() = default;

    private:
        void PollEvent(bool &quit) override;

        bool Init(const Descriptor &desc) override;
    };


}
