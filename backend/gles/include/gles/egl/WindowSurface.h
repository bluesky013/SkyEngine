//
// Created by Zach Lee on 2023/4/1.
//

#pragma once

#include <memory>
#include <gles/Forward.h>
#include <gles/egl/Config.h>
#include <gles/egl/Surface.h>

namespace sky::gles {

    class WindowSurface : public Surface {
    public:
        WindowSurface() = default;
        ~WindowSurface() = default;

        bool Init(EGLConfig config, void *window);
        const rhi::Extent2D &GetExtent() const { return extent; }

    private:
        rhi::Extent2D extent;
    };
}


