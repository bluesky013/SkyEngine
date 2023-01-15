//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class BufferView {
    public:
        BufferView() = default;
        ~BufferView() = default;

        struct Descriptor {
            PixelFormat format = PixelFormat::UNDEFINED;
            uint64_t offset = 0;
            uint64_t range  = 0;
        };

    protected:
        Descriptor viewDesc;
    };

}