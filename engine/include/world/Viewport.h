//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

namespace sky {

    class Viewport {
    public:
        Viewport(void* hWnd) : window(hWnd) {}
        ~Viewport() = default;

        void* GetNativeWindow() const;

    private:
        void* window;
    };

}