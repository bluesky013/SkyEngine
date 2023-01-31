//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Buffer.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class Buffer : public rhi::Buffer, public DevObject {
    public:
        Buffer(Device &dev) : DevObject(dev) {};
        ~Buffer();

        bool Init(const Descriptor &desc);

    };

}
