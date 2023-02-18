//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <memory>
#include <gles/Forward.h>
#include <gles/Config.h>
#include <gles/Surface.h>

namespace sky::gles {

    class PBuffer : public Surface {
    public:
        PBuffer() = default;
        ~PBuffer() = default;

        bool Init(EGLConfig config);
    };
}
