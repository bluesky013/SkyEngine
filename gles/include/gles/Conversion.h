//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <gles/Forward.h>

namespace sky::gles {

    struct InternalFormat {
        GLenum internal = 0;
        GLenum format   = 0;
        GLenum type     = 0;
    };

    struct FormatFeature {
        bool texture      = false;
        bool renderBuffer = false;
        bool compressed   = false;
    };

    const InternalFormat &GetInternalFormat(rhi::PixelFormat format);
    const FormatFeature &GetFormatFeature(rhi::PixelFormat format);

}
