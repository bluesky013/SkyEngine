//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Buffer.h>
#include <mtl/DevObject.h>

namespace sky::mtl {
    class Device;

    class Buffer : public rhi::Buffer, public DevObject, public std::enable_shared_from_this<Buffer> {
    public:
        Buffer(Device &dev) : DevObject(dev) {};
        rhi::BufferViewPtr CreateView(const rhi::BufferViewDesc &desc) override;
    private:
        friend class Device;
        bool Init(const Descriptor &);
    };
    using BufferPtr = std::shared_ptr<Buffer>;
}
