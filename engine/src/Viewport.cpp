//
// Created by Zach Lee on 2021/11/14.
//


#include <world/Viewport.h>

namespace sky {

    void* Viewport::GetNativeWindow() const
    {
        return window;
    }

}