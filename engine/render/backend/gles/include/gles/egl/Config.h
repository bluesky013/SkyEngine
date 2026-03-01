//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <gles/Forward.h>

namespace sky::gles {

    struct Config {
        int32_t rgb     = 8;
        int32_t alpha   = 8;
        int32_t depth   = 24;
        int32_t stencil = 8;
        int32_t sample  = 0;
    };
}
