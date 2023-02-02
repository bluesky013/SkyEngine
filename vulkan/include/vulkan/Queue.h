//
// Created by Zach Lee on 2021/11/7.
//

#pragma once

#include "vulkan/CommandPool.h"
#include "vulkan/vulkan.h"
#include "rhi/Queue.h"
#include <thread>
#include <unordered_map>

namespace sky::vk {

    class Device;

    class Queue : public rhi::Queue, public DevObject {
    public:
        ~Queue() = default;

        void Setup();

        uint32_t GetQueueFamilyIndex() const
        {
            return queueFamilyIndex;
        }

        VkQueue GetNativeHandle() const
        {
            return queue;
        }

        CommandBufferPtr AllocateCommandBuffer(const CommandBuffer::VkDescriptor &desc);
        CommandBufferPtr AllocateTlsCommandBuffer(const CommandBuffer::VkDescriptor &desc);

        void WaitIdle();
    private:
        const CommandPoolPtr &GetOrCreatePool();

        friend class Device;
        Queue(Device &dev, VkQueue q, uint32_t family) : DevObject(dev), queueFamilyIndex(family), queue(q)
        {
        }

        uint32_t       queueFamilyIndex;
        VkQueue        queue;
        CommandPoolPtr pool;

        mutable std::mutex                                  mutex;
        std::unordered_map<std::thread::id, CommandPoolPtr> tlsPools;
    };

    using QueuePtr = std::unique_ptr<Queue>;
} // namespace sky::vk
