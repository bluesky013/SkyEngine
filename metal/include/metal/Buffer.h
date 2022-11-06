//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <metal/DevObject.h>
#include <Metal/MTLBuffer.hpp>

namespace sky::mtl {
    class Device;

    class Buffer : public DevObject {
    public:
        struct Descriptor {
            uint32_t size;
        };

        ~Buffer() = default;

    private:
        Buffer(Device &);

        bool Init(const Descriptor &);

        Descriptor info = {};
        MTL::Buffer *buffer = nullptr;
    };
}
