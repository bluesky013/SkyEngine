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
#include <vulkan/DrawItem.h>
#include <vulkan/RenderPass.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/QueryPool.h>
#include <vector>

namespace sky::drv {

    class Device;
    class Queue;

    struct PassBeginInfo {
        drv::FrameBufferPtr frameBuffer;
        drv::RenderPassPtr renderPass;
        uint32_t clearValueCount = 0;
        VkClearValue* clearValues = nullptr;
        VkRect2D* renderArea = nullptr;
    };

    class GraphicsEncoder {
    public:
        GraphicsEncoder();
        ~GraphicsEncoder() = default;

        void Encode(const DrawItem& item);

        void BeginPass(const PassBeginInfo&);

        void BindPipeline(const GraphicsPipelinePtr &pso);

        void BindShaderResource(const DescriptorSetBinderPtr &binder);

        void BindAssembly(const VertexAssemblyPtr &assembly);

        void PushConstant(const PushConstantsPtr &constants);

        void SetViewport(uint32_t count, const VkViewport *viewport);

        void SetScissor(uint32_t count, const VkRect2D *scissor);

        void DrawIndexed(const CmdDrawIndexed &indexed);

        void DrawLinear(const CmdDrawLinear &linear);

        void EndPass();

    private:
        friend class CommandBuffer;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo vkBeginInfo = {};
    };

    class CommandBuffer : public DevObject {
    public:
        ~CommandBuffer() override;

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

        void ImageBarrier(const ImagePtr &image, const VkImageSubresourceRange& subresourceRange, const Barrier& barrier, VkImageLayout src, VkImageLayout dst);

        void BufferBarrier(const BufferPtr &buffer, const Barrier& barrier, uint32_t size, uint32_t offset);

        void Copy(VkImage src, VkImageLayout srcLayout,
            VkImage dst, VkImageLayout dstLayout, const VkImageCopy& copy);

        void Copy(const BufferPtr &src, const ImagePtr &dst, const VkBufferImageCopy& copy);

        void Copy(const BufferPtr &src, const BufferPtr &dst, const VkBufferCopy& copy);

        void BeginQuery(const QueryPoolPtr &pool, uint32_t queryId);

        void EndQuery(const QueryPoolPtr &pool, uint32_t queryId);

        void ResetQueryPool(const QueryPoolPtr &pool, uint32_t first, uint32_t count);

        void End();

        struct SubmitInfo {
            std::vector<std::pair<VkPipelineStageFlags, SemaphorePtr>> waits;
            std::vector<SemaphorePtr> submitSignals;
        };

        void Submit(Queue& queue, const SubmitInfo& submit);

        GraphicsEncoder EncodeGraphics();

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