//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include "framework/window/NativeWindow.h"

namespace sky {

    class AndroidWindow : public NativeWindow {
    public:
        AndroidWindow() = default;
        ~AndroidWindow() override = default;

    private:
        void PollEvent(bool &quit);

        bool Init(const Descriptor &desc) override;
    };


}
