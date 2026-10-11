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

        // The OS window handle may be (re)created on resume; keep the engine
        // window bound to the live ANativeWindow.
        void SetWinHandle(void *handle)
        {
            winHandle = handle;
        }

        // Called on WINDOW_RESIZED / CONFIG_CHANGED so GetWidth/GetHeight report
        // the live surface size.
        void NotifyResized(uint32_t w, uint32_t h)
        {
            descriptor.width  = w;
            descriptor.height = h;
        }

    private:
        void PollEvent(bool &quit);

        bool Init(const Descriptor &desc) override;
    };


}
