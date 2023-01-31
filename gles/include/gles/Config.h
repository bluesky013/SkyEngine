//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <gles/Forward.h>

namespace sky::gles {

    struct Config {
        EGLint rgb     = 8;
        EGLint alpha   = 8;
        EGLint depth   = 24;
        EGLint stencil = 8;
        EGLint sample  = 0;
    };
}
