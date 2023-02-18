//
// Created by Zach Lee on 2023/1/30.
//

#include <dx12/CommandPool.h>
#include <dx12/Device.h>

namespace sky::dx {

    CommandPool::CommandPool(Device &dev) : DevObject(dev)
    {
    }

    CommandPool::~CommandPool()
    {
    }


    bool CommandPool::Init(const Descriptor &desc)
    {
        if (!SUCCEEDED(device.GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(allocator.GetAddressOf())))) {
            return false;
        }

        return true;
    }

    CommandBufferPtr CommandPool::Allocate(const CommandBuffer::Descriptor &desc)
    {
        auto cmdBuffer = std::make_shared<CommandBuffer>(device);
        if (cmdBuffer->Init(desc, allocator, type)) {
            return cmdBuffer;
        }
        return {};
    }

}