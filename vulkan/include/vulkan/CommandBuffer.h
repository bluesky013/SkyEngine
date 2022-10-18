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

namespace sky::drv {

    class Device;
    class Queue;
    class CommandBuffer;
    class SecondaryCommands;

    struct PassBeginInfo {
        drv::FrameBufferPtr frameBuffer;
        drv::RenderPassPtr  renderPass;
        uint32_t            clearValueCount = 0;
        VkClearValue       *clearValues     = nullptr;
        VkRect2D           *renderArea      = nullptr;
        VkSubpassContents   contents        = VK_SUBPASS_CONTENTS_INLINE;
    };

    class GraphicsEncoder {
    public:
        GraphicsEncoder(CommandBuffer &);
        ~GraphicsEncoder() = default;

        void Encode(const DrawItem &item);

        void BeginPass(const PassBeginInfo &);

        void BindPipeline(const GraphicsPipelinePtr &pso);

        void BindShaderResource(const DescriptorSetBinderPtr &binder);

        void BindAssembly(const VertexAssemblyPtr &assembly);

        void PushConstant(const PushConstantsPtr &constants);

        void SetViewport(uint32_t count, const VkViewport *viewport);

        void SetScissor(uint32_t count, const VkRect2D *scissor);

        void DrawIndexed(const CmdDrawIndexed &indexed);

        void DrawLinear(const CmdDrawLinear &linear);

        void DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size);

        void EndPass();

        const VkRenderPassBeginInfo &GetCurrentPass() const;

        uint32_t GetSubPassId() const;

        const VkViewport &GetCurrentViewport() const;

        const VkRect2D &GetCurrentScissor() const;

        CommandBuffer &GetCommandBuffer();

    private:
        friend class CommandBuffer;
        CommandBuffer        &cmdBuffer;
        VkCommandBuffer       cmd              = VK_NULL_HANDLE;
        VkRenderPassBeginInfo vkBeginInfo      = {};
        VkViewport            viewport{};
        VkRect2D              scissor{};

        uint32_t              currentSubPassId = 0;
        VkPipeline            currentPso       = VK_NULL_HANDLE;
        VertexAssemblyPtr     currentAssembler;
    };

    class CommandBuffer : public DevObject {
    public:
        ~CommandBuffer() override;

        struct Descriptor {
            VkCommandBufferLevel level     = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bool                 needFence = true;
        };

        void Wait(uint64_t timeout = UINT64_MAX);

        void Begin();

        void Begin(const VkCommandBufferInheritanceInfo &inheritanceInfo);

        template <typename Func>
        void Encode(Func &&fun)
        {
            fun(cmdBuffer);
        }

        void ImageBarrier(
            const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst);

        void BufferBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset);

        void Copy(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy &copy);

        void Copy(const BufferPtr &src, const ImagePtr &dst, const VkBufferImageCopy &copy);

        void Copy(const BufferPtr &src, const BufferPtr &dst, const VkBufferCopy &copy);

        void BeginQuery(const QueryPoolPtr &pool, uint32_t queryId);

        void EndQuery(const QueryPoolPtr &pool, uint32_t queryId);

        void ResetQueryPool(const QueryPoolPtr &pool, uint32_t first, uint32_t count);

        void End();

        struct SubmitInfo {
            std::vector<std::pair<VkPipelineStageFlags, SemaphorePtr>> waits;
            std::vector<SemaphorePtr>                                  submitSignals;
        };

        void Submit(Queue &queue, const SubmitInfo &submit);

        void ExecuteSecondary(const SecondaryCommands &);

        GraphicsEncoder EncodeGraphics();

        VkCommandBuffer GetNativeHandle() const;

    private:
        friend class CommandPool;
        CommandBuffer(Device &, VkCommandPool, VkCommandBuffer);

        bool Init(const Descriptor &);

        VkCommandPool   pool;
        VkCommandBuffer cmdBuffer;
        FencePtr        fence;
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

} // namespace sky::drv