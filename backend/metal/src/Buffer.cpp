//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Buffer.h>
#include <mtl/Device.h>
#include <mtl/BufferView.h>

namespace sky::mtl {
    bool Buffer::Init(const Descriptor &desc)
    {
        bufferDesc = desc;
        auto *mtlDevice = device.GetMetalDevice();

        MTL::ResourceOptions options = {};
        buffer = mtlDevice->newBuffer(desc.size, options);
    }

    rhi::BufferViewPtr Buffer::CreateView(const rhi::BufferViewDesc &desc)
    {
        BufferViewPtr ret = std::make_shared<BufferView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::BufferView>(ret);
    }
}
