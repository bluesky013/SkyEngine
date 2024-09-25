//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/DevObject.h>
#include <vulkan/DrawItem.h>
#include <vulkan/Fence.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/Image.h>
#include <vulkan/QueryPool.h>
#include <vulkan/RenderPass.h>
#include <vulkan/Semaphore.h>
#include <vulkan/vulkan.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/ComputePipeline.h>
#include <rhi/CommandBuffer.h>

namespace sky::vk {

    class Device;
    class Queue;
    class CommandBuffer;
    class SecondaryCommands;

    struct PassBeginInfo {
        vk::FrameBufferPtr frameBuffer;
        vk::RenderPassPtr  renderPass;
        uint32_t            clearValueCount = 0;
        VkClearValue       *clearValues     = nullptr;
        VkRect2D           *renderArea      = nullptr;
        VkSubpassContents   contents        = VK_SUBPASS_CONTENTS_INLINE;
    };

    class ComputeEncoder : public rhi::ComputeEncoder {
    public:
        explicit ComputeEncoder(CommandBuffer &);
        ~ComputeEncoder() override = default;

        void BindShaderResource(const DescriptorSetBinderPtr &binder);
        void BindPipeline(const ComputePipelinePtr &pso);
        void Dispatch(uint32_t x, uint32_t y, uint32_t z);

    private:
        friend class CommandBuffer;
        CommandBuffer        &cmdBuffer;
        VkCommandBuffer       cmd              = VK_NULL_HANDLE;
        VkPipeline            currentPso       = VK_NULL_HANDLE;
    };

    class GraphicsEncoder : public rhi::GraphicsEncoder {
    public:
        explicit GraphicsEncoder(CommandBuffer &);
        ~GraphicsEncoder() override = default;

        void BindShaderResource(const DescriptorSetBinderPtr &binder);
        void Encode(const DrawItem &item);
        void BeginPass(const PassBeginInfo &);
        void BindPipeline(const GraphicsPipelinePtr &pso);
        void BindAssembly(const VertexAssemblyPtr &assembly);
        void PushConstant(const PushConstantsPtr &constants);
        void SetViewport(uint32_t count, const VkViewport *viewport);
        void SetScissor(uint32_t count, const VkRect2D *scissor);
        void DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size);

        rhi::GraphicsEncoder &BeginQuery(const rhi::QueryPoolPtr &query, uint32_t id) override;
        rhi::GraphicsEncoder &EndQuery(const rhi::QueryPoolPtr &query, uint32_t id) override;
        rhi::GraphicsEncoder &WriteTimeStamp(const rhi::QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id) override;
        rhi::GraphicsEncoder &BeginPass(const rhi::PassBeginInfo &info) override;
        rhi::GraphicsEncoder &BindPipeline(const rhi::GraphicsPipelinePtr &pso) override;
        rhi::GraphicsEncoder &BindAssembly(const rhi::VertexAssemblyPtr &assembly) override;
        rhi::GraphicsEncoder &BindVertexBuffers(const std::vector<rhi::BufferView> &vbs) override;
        rhi::GraphicsEncoder &BindIndexBuffer(const rhi::BufferView& view, rhi::IndexType type) override;
        rhi::GraphicsEncoder &SetViewport(uint32_t count, const rhi::Viewport *viewport) override;
        rhi::GraphicsEncoder &SetScissor(uint32_t count, const rhi::Rect2D *scissor) override;
        rhi::GraphicsEncoder &DrawIndexed(const rhi::CmdDrawIndexed &indexed) override;
        rhi::GraphicsEncoder &DrawLinear(const rhi::CmdDrawLinear &linear) override;
        rhi::GraphicsEncoder &DrawIndexedIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &BindSet(uint32_t id, const rhi::DescriptorSetPtr &set) override;
        rhi::GraphicsEncoder &SetOffset(uint32_t set, uint32_t binding, uint32_t index, uint32_t offset) override;
        rhi::GraphicsEncoder &NextSubPass() override;
        rhi::GraphicsEncoder &EndPass() override;

    private:
        friend class CommandBuffer;
        CommandBuffer        &cmdBuffer;
        VkCommandBuffer       cmd              = VK_NULL_HANDLE;
        VkPipeline            currentPso       = VK_NULL_HANDLE;
        VkRenderPassBeginInfo vkBeginInfo      = {};
        VkViewport            viewport{};
        VkRect2D              scissor{};
        VertexAssemblyPtr     currentAssembler;
        DescriptorSetBinder   currentBinder;
    };

    class BlitEncoder : public rhi::BlitEncoder {
    public:
        explicit BlitEncoder(CommandBuffer &cmd) : cmdBuffer(cmd) {}
        ~BlitEncoder() override = default;

        rhi::BlitEncoder &CopyTexture() override;
        rhi::BlitEncoder &CopyTextureToBuffer() override;
        rhi::BlitEncoder &CopyBufferToTexture() override;
        rhi::BlitEncoder &BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter) override;
        rhi::BlitEncoder &ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::ResolveInfo> &resolveInputs) override;
        rhi::BlitEncoder &CopyBuffer(const rhi::BufferPtr &src, const rhi::BufferPtr &dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) override;
    private:
        friend class CommandBuffer;
        CommandBuffer        &cmdBuffer;
        VkResolveImageInfo2  resolveInfo = {VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2, nullptr};
        VkBlitImageInfo2     blitInfo = {VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, nullptr};
        std::vector<VkImageResolve2> resolves;
        std::vector<VkImageBlit2> blit;
    };

    class CommandBuffer : public rhi::CommandBuffer, public DevObject {
    public:
        explicit CommandBuffer(Device &);
        ~CommandBuffer() override;

        struct VkDescriptor {
            VkCommandBufferLevel level     = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bool                 needFence = true;
        };

        template <typename Func>
        void Encode(Func &&fun)
        {
            fun(cmdBuffer);
        }

        void ImageBarrier(
            const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst);
        void BufferBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset);
        void QueueBarrier(const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst);
        void QueueBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset);

        void QueueBarrier(const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const AccessInfo &src, const AccessInfo &dst);
        void QueueBarrier(const BufferPtr &buffer, uint64_t offset, uint64_t size, const AccessInfo &src, const AccessInfo &dst);

        void Copy(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy &copy);
        void Copy(const BufferPtr &src, const ImagePtr &dst, const VkBufferImageCopy &copy);
        void Copy(const BufferPtr &src, const BufferPtr &dst, const VkBufferCopy &copy);

        void BeginQuery(const QueryPoolPtr &pool, uint32_t queryId);
        void EndQuery(const QueryPoolPtr &pool, uint32_t queryId);
        void ResetQueryPool(const QueryPoolPtr &queryPool, uint32_t first, uint32_t count);

        struct SubmitInfo {
            std::vector<std::pair<VkPipelineStageFlags, SemaphorePtr>> waits;
            std::vector<SemaphorePtr>                                  submitSignals;
            FencePtr                                                   fence;
        };

        // vk
        void Begin(const VkCommandBufferInheritanceInfo &inheritanceInfo);
        void Submit(Queue &queue, const SubmitInfo &submit);

        // rhi
        void Begin() override;
        void End() override;
        void Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit) override;
        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override;
        std::shared_ptr<rhi::BlitEncoder> EncodeBlit() override;
        void QueueBarrier(const rhi::ImageBarrier &imageBarrier) override;
        void QueueBarrier(const rhi::ImagePtr &image, const rhi::ImageSubRange &range, const rhi::BarrierInfo &barrierInfo) override;
        void QueueBarrier(const rhi::BufferPtr &buffer, uint64_t offset, uint64_t range, const rhi::BarrierInfo &barrierInfo) override;
        void FlushBarriers() override;
        void ResetQueryPool(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count) override;
        void GetQueryResult(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count,
           const rhi::BufferPtr &result, uint32_t offset, uint32_t stride) override;

        void ExecuteSecondary(const SecondaryCommands &);
        GraphicsEncoder EncodeVkGraphics();
        ComputeEncoder EncodeVKCompute();

        VkCommandBuffer GetNativeHandle() const;
    private:
        friend class CommandPool;
        friend class Device;

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkCommandPool   pool;
        VkCommandBuffer cmdBuffer;
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkImageMemoryBarrier> imageBarriers;
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = 0;
    };

    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;

    class SecondaryCommands {
    public:
        SecondaryCommands()  = default;
        ~SecondaryCommands() = default;

        void Emplace(const CommandBufferPtr &cmd);

        void OnExecute(VkCommandBuffer main) const;

    private:
        mutable std::mutex           mutex;
        std::list<CommandBufferPtr>  cmdBuffers;
        std::vector<VkCommandBuffer> cmdHandlers;
    };

} // namespace sky::vk
