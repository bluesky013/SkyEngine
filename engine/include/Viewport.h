//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <core/math/Rect.h>

namespace sky {

    class Viewport {
    public:
        Viewport();
        ~Viewport() = default;

        void* GetNativeWindow() const;

    private:
        void* window;
        Rect rect;
    };

}