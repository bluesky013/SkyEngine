//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <vulkan/Fence.h>
#include <vulkan/Semaphore.h>
#include <vulkan/Buffer.h>
#include <vulkan/Image.h>
#include <vector>

namespace sky::drv {

    class Device;
    class Queue;

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

        void ImageBarrier(ImagePtr image, const VkImageSubresourceRange& subresourceRange, const Barrier& barrier, VkImageLayout src, VkImageLayout dst);

        void Copy(VkImage src, VkImageLayout srcLayout,
            VkImage dst, VkImageLayout dstLayout, const VkImageCopy& copy);

        void Copy(BufferPtr src, ImagePtr dst, const VkBufferImageCopy& copy);

        void Copy(BufferPtr src, BufferPtr dst, const VkBufferCopy& copy);

        void End();

        struct SubmitInfo {
            std::vector<std::pair<VkPipelineStageFlags, SemaphorePtr>> waits;
            std::vector<SemaphorePtr> submitSignals;
        };

        void Submit(Queue& queue, const SubmitInfo& submit);

        VkCommandBuffer GetNativeHandle() const;

    private:
        friend class CommandPool;
        CommandBuffer(Device&, VkCommandPool, VkCommandBuffer);

        bool Init(const Descriptor&);

        VkCommandPool pool;
        VkCommandBuffer cmdBuffer;
        FencePtr fence;
    };

    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;

}