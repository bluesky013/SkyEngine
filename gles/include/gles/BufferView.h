//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/BufferView.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &dev) : DevObject(dev) {}
        ~BufferView() = default;

        bool Init(const Descriptor &desc);
    };

}

