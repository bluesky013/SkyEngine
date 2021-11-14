//
// Created by Zach Lee on 2021/11/14.
//


#include <Viewport.h>

namespace sky {

    Viewport::Viewport() : window(nullptr), rect{0, 0, 1, 1}
    {
    }

    void* Viewport::GetNativeWindow() const
    {
        return window;
    }

}