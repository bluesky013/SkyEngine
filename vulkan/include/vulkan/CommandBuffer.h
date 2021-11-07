//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::drv {

    class Device;
    class Fence;
    class Queue;
    class Semaphore;

    class CommandBuffer : public DevObject {
    public:
        ~CommandBuffer();

        struct Descriptor {
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bool needFence = true;
        };

        void Wait(uint64_t timeout = UINT64_MAX);

        void Begin();

        template <typename Func>
        void Encode(Func&& fun)
        {
            fun(cmdBuffer);
        }

        void End();

        struct SubmitInfo {
            std::vector<std::pair<VkPipelineStageFlags, Semaphore*>> waits;
            std::vector<Semaphore*> signals;
        };

        void Submit(Queue& queue, const SubmitInfo& submit);

    private:
        friend class CommandPool;
        CommandBuffer(Device&, VkCommandPool, VkCommandBuffer);

        bool Init(const Descriptor&);

        VkCommandPool pool;
        VkCommandBuffer cmdBuffer;
        Fence* fence;
    };

}