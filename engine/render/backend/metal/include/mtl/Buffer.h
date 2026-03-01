//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Buffer.h>
#include <mtl/DevObject.h>
#import <metal/MTLBuffer.h>

namespace sky::mtl {
    class Device;

    class Buffer : public rhi::Buffer, public DevObject, public std::enable_shared_from_this<Buffer> {
    public:
        Buffer(Device &dev) : DevObject(dev) {};
        ~Buffer();

        uint8_t *Map() override;
        void UnMap() override;

        id<MTLBuffer> GetNativeHandle() const { return buffer; }

    private:
        friend class Device;
        bool Init(const Descriptor &);

        id<MTLBuffer> buffer;
    };
    using BufferPtr = std::shared_ptr<Buffer>;
}
