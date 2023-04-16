//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Buffer.h>
#include <mtl/DevObject.h>

#include <Metal/MTLBuffer.hpp>

namespace sky::mtl {
    class Device;

    class Buffer : public rhi::Buffer, public DevObject {
    public:
        ~Buffer() = default;

    private:
        Buffer(Device &);
        bool Init(const Descriptor &);

        MTL::Buffer *buffer = nullptr;
    };
}
