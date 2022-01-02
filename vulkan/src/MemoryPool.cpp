//
// Created by Zach Lee on 2022/1/2.
//

#include <vulkan/MemoryPool.h>
#include <vulkan/Device.h>

namespace sky::drv {

    MemoryPool::MemoryPool(Device& dev) : DevObject(dev), pool(VK_NULL_HANDLE)
    {

    }

    bool MemoryPool::Init(const Descriptor& des)
    {
        VmaPoolCreateInfo poolInfo = {};
        vmaCreatePool(device.GetAllocator(), &poolInfo,  &pool);
    }

}