//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <gles/Forward.h>

namespace sky::gles {

    struct FormatType {
        GLenum internal = 0;
        GLenum format   = 0;
        GLenum type     = 0;
    };

    const FormatType &GetFormatInfo(rhi::PixelFormat format);


}