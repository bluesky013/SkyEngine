//
// Created by Zach Lee on 2022/1/2.
//

#include <vulkan/Device.h>
#include <vulkan/MemoryPool.h>

namespace sky::vk {

    MemoryPool::MemoryPool(Device &dev) : DevObject(dev), pool(VK_NULL_HANDLE)
    {
    }

    bool MemoryPool::Init(const Descriptor &des)
    {
        VmaPoolCreateInfo poolInfo = {};
        vmaCreatePool(device.GetAllocator(), &poolInfo, &pool);
        return true;
    }

} // namespace sky::vk
