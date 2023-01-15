//
// Created by Zach Lee on 2022/1/3.
//

#include <vulkan/Device.h>
#include <vulkan/Queue.h>

namespace sky::vk {

    void Queue::Setup()
    {
        CommandPool::VkDescriptor des = {};
        des.queueFamilyIndex        = queueFamilyIndex;
        des.flag                    = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        pool = device.CreateDeviceObject<CommandPool>(des);
    }

    CommandBufferPtr Queue::AllocateCommandBuffer(const CommandBuffer::Descriptor &des)
    {
        return pool->Allocate(des);
    }

    CommandBufferPtr Queue::AllocateTlsCommandBuffer(const CommandBuffer::Descriptor &des)
    {
        auto &tlsPool = GetOrCreatePool();
        return tlsPool->Allocate(des);
    }

    const CommandPoolPtr &Queue::GetOrCreatePool()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto                       &pool = tlsPools[std::this_thread::get_id()];
        if (!pool) {
            pool = device.CreateDeviceObject<CommandPool>({});
        }
        return pool;
    }

    void Queue::WaitIdle()
    {
        vkQueueWaitIdle(queue);
    }

} // namespace sky::vk
