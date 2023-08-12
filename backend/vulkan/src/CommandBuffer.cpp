//
// Created by Zach Lee on 2021/11/7.
//
#include "vulkan/CommandBuffer.h"
#include "core/logger/Logger.h"
#include "vulkan/Device.h"
#include "vulkan/Fence.h"
#include "vulkan/Conversion.h"
#include "vulkan/Barrier.h"
#include "vulkan/Ext.h"

static const char *TAG = "Vulkan";

namespace sky::vk {

    static VkImageLayout LAYOUT_MAP[] = {
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    UNDEFINED
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    INDIRECT_COMMAND_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    INDEX_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    VERTEX_ATTRIBUTE_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    UNIFORM_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                         //    INPUT_ATTACHMENT_READ
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,          //    SHADER_READ
        VK_IMAGE_LAYOUT_GENERAL,                           //    SHADER_WRITE
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,          //    COLOR_READ
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,          //    COLOR_WRITE
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,  //    DEPTH_STENCIL_READ
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, //    DEPTH_STENCIL_WRITE
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,             //    TRANSFER_READ
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,             //    TRANSFER_WRITE
        VK_IMAGE_LAYOUT_UNDEFINED,                        //    HOST_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                        //    HOST_WRITE
        VK_IMAGE_LAYOUT_UNDEFINED,                        //    MEMORY_READ
        VK_IMAGE_LAYOUT_UNDEFINED,                        //    MEMORY_WRITE
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                  //    PRESENT
    };

    static bool IsDepthStencil(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
            format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT;
    }

    CommandBuffer::CommandBuffer(Device &dev) : DevObject(dev), pool(VK_NULL_HANDLE), cmdBuffer(VK_NULL_HANDLE)
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        if (pool != VK_NULL_HANDLE && cmdBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device.GetNativeHandle(), pool, 1, &cmdBuffer);
        }
    }

    bool CommandBuffer::Init(const Descriptor &des)
    {
        return true;
    }

    bool CommandBuffer::Init(const VkDescriptor &des)
    {
        return true;
    }

    void CommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void CommandBuffer::Begin(const VkCommandBufferInheritanceInfo &inheritanceInfo)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo         = &inheritanceInfo;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void CommandBuffer::BeginQuery(const QueryPoolPtr &queryPool, uint32_t queryId)
    {
        vkCmdBeginQuery(cmdBuffer, queryPool->GetNativeHandle(), queryId, 0);
    }

    void CommandBuffer::EndQuery(const QueryPoolPtr &queryPool, uint32_t queryId)
    {
        vkCmdEndQuery(cmdBuffer, queryPool->GetNativeHandle(), queryId);
    }

    void CommandBuffer::ResetQueryPool(const QueryPoolPtr &queryPool, uint32_t first, uint32_t count)
    {
        vkCmdResetQueryPool(cmdBuffer, queryPool->GetNativeHandle(), first, count);
    }

    void CommandBuffer::End()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    void CommandBuffer::Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit)
    {
        SubmitInfo vkSubmitInfo = {};
        for (const auto &[stage, semaphore] : submit.waits) {
            vkSubmitInfo.waits.emplace_back(FromRHI(stage), std::static_pointer_cast<Semaphore>(semaphore));
        }
        for (const auto &sig : submit.submitSignals) {
            vkSubmitInfo.submitSignals.emplace_back(std::static_pointer_cast<Semaphore>(sig));
        }
        vkSubmitInfo.fence = std::static_pointer_cast<Fence>(submit.fence);
        Submit(static_cast<Queue&>(queue), vkSubmitInfo);
    }

    std::shared_ptr<rhi::GraphicsEncoder> CommandBuffer::EncodeGraphics()
    {
        return std::make_shared<GraphicsEncoder>(*this);
    }

    std::shared_ptr<rhi::BlitEncoder> CommandBuffer::EncodeBlit()
    {
        return std::make_shared<BlitEncoder>(*this);
    }

    void CommandBuffer::QueueBarrier(const rhi::ImageBarrier &imageBarrier)
    {
        const auto &view = std::static_pointer_cast<ImageView>(imageBarrier.view);
        const auto &srcAccess = device.GetAccessInfo(imageBarrier.srcFlags);
        const auto &dstAccess = device.GetAccessInfo(imageBarrier.dstFlags);
        QueueBarrier(view->GetImage(), view->GetSubRange(), srcAccess, dstAccess);
    }

    void CommandBuffer::QueueBarrier(const rhi::ImagePtr &image, const rhi::ImageSubRange &range, const rhi::BarrierInfo &barrierInfo)
    {
        const auto &img = std::static_pointer_cast<Image>(image);
        const auto &srcAccess = device.GetAccessInfo(barrierInfo.srcFlags);
        const auto &dstAccess = device.GetAccessInfo(barrierInfo.dstFlags);
        QueueBarrier(img, VkImageSubresourceRange{FromRHI(range.aspectMask), range.baseLevel, range.levels, range.baseLayer, range.layers},
                     srcAccess, dstAccess);
    }

    void CommandBuffer::QueueBarrier(const rhi::BufferPtr &buffer, uint64_t offset, uint64_t range, const rhi::BarrierInfo &barrierInfo)
    {
        const auto &buf = std::static_pointer_cast<Buffer>(buffer);
        const auto &srcAccess = device.GetAccessInfo(barrierInfo.srcFlags);
        const auto &dstAccess = device.GetAccessInfo(barrierInfo.dstFlags);
        QueueBarrier(buf, offset, range, srcAccess, dstAccess);
    }

    void CommandBuffer::ExecuteSecondary(const SecondaryCommands &buffers)
    {
        buffers.OnExecute(cmdBuffer);
    }

    void CommandBuffer::Submit(Queue &queue, const SubmitInfo &submit)
    {
        auto                              waitSize = static_cast<uint32_t>(submit.waits.size());
        std::vector<VkPipelineStageFlags> waitStages(waitSize);
        std::vector<VkSemaphore>          waitSemaphores(waitSize);
        for (uint32_t i = 0; i < waitSize; ++i) {
            waitStages[i]     = submit.waits[i].first;
            waitSemaphores[i] = submit.waits[i].second->GetNativeHandle();
        }

        auto                     signalSize = static_cast<uint32_t>(submit.submitSignals.size());
        std::vector<VkSemaphore> signalSemaphores(signalSize);
        for (uint32_t i = 0; i < signalSize; ++i) {
            signalSemaphores[i] = submit.submitSignals[i]->GetNativeHandle();
        }

        VkSubmitInfo submitInfo       = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmdBuffer;

        if (!submit.submitSignals.empty()) {
            submitInfo.signalSemaphoreCount = signalSize;
            submitInfo.pSignalSemaphores    = signalSemaphores.data();
        }

        if (!waitStages.empty()) {
            submitInfo.waitSemaphoreCount = waitSize;
            submitInfo.pWaitDstStageMask  = waitStages.data();
            submitInfo.pWaitSemaphores    = waitSemaphores.data();
        }
        vkQueueSubmit(queue.GetNativeHandle(), 1, &submitInfo, submit.fence ? submit.fence->GetNativeHandle() : VK_NULL_HANDLE);
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle() const
    {
        return cmdBuffer;
    }

    GraphicsEncoder CommandBuffer::EncodeVkGraphics()
    {
        return GraphicsEncoder{*this};
    }

    ComputeEncoder CommandBuffer::EncodeVKCompute()
    {
        return ComputeEncoder{*this};
    }

    void CommandBuffer::Copy(const BufferPtr &src, const ImagePtr &dst, const VkBufferImageCopy &copy)
    {
        vkCmdCopyBufferToImage(cmdBuffer, src->GetNativeHandle(), dst->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    void CommandBuffer::ImageBarrier(
        const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst)
    {
        QueueBarrier(image, subresourceRange, barrier, src, dst);
        FlushBarriers();
    }

    void CommandBuffer::BufferBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset)
    {
        QueueBarrier(buffer, barrier, size, offset);
        FlushBarriers();
    }

    void CommandBuffer::QueueBarrier(const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst)
    {
        imageBarriers.emplace_back(VkImageMemoryBarrier{});
        auto &imageMemoryBarrier = imageBarriers.back();
        imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask        = barrier.srcAccessMask;
        imageMemoryBarrier.dstAccessMask        = barrier.dstAccessMask;
        imageMemoryBarrier.image                = image->GetNativeHandle();
        imageMemoryBarrier.subresourceRange     = subresourceRange;
        imageMemoryBarrier.oldLayout            = src;
        imageMemoryBarrier.newLayout            = dst;
        imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        srcStageMask |= barrier.srcStageMask;
        dstStageMask |= barrier.dstStageMask;
    }

    void CommandBuffer::QueueBarrier(const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const AccessInfo &src, const AccessInfo &dst)
    {
        imageBarriers.emplace_back(VkImageMemoryBarrier{});
        auto &imageMemoryBarrier = imageBarriers.back();
        imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask        = src.accessFlags;
        imageMemoryBarrier.dstAccessMask        = dst.accessFlags;
        imageMemoryBarrier.image                = image->GetNativeHandle();
        imageMemoryBarrier.subresourceRange     = subresourceRange;
        imageMemoryBarrier.oldLayout            = src.imageLayout;
        imageMemoryBarrier.newLayout            = dst.imageLayout;
        imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        srcStageMask |= src.pipelineStages;
        dstStageMask |= dst.pipelineStages;
    }

    void CommandBuffer::QueueBarrier(const BufferPtr &buffer, uint64_t offset, uint64_t size, const AccessInfo &src, const AccessInfo &dst)
    {
        bufferBarriers.emplace_back(VkBufferMemoryBarrier{});
        auto &bufferBarrier = bufferBarriers.back();
        bufferBarrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.srcAccessMask         = src.accessFlags;
        bufferBarrier.dstAccessMask         = dst.accessFlags;
        bufferBarrier.buffer                = buffer->GetNativeHandle();
        bufferBarrier.offset                = offset;
        bufferBarrier.size                  = size;
        bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        srcStageMask |= src.pipelineStages;
        dstStageMask |= dst.pipelineStages;
    }

    void CommandBuffer::QueueBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset)
    {
        bufferBarriers.emplace_back(VkBufferMemoryBarrier{});
        auto &bufferBarrier = bufferBarriers.back();
        bufferBarrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.srcAccessMask         = barrier.srcAccessMask;
        bufferBarrier.dstAccessMask         = barrier.dstAccessMask;
        bufferBarrier.buffer                = buffer->GetNativeHandle();
        bufferBarrier.offset                = offset;
        bufferBarrier.size                  = size;
        bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        srcStageMask |= barrier.srcStageMask;
        dstStageMask |= barrier.dstStageMask;
    }

    void CommandBuffer::FlushBarriers()
    {
        if (bufferBarriers.empty() && imageBarriers.empty()) {
            return;
        }
        vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
                             0, nullptr,
                             static_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
                             static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
        bufferBarriers.clear();
        imageBarriers.clear();
        srcStageMask = 0;
        dstStageMask = 0;
    }

    void CommandBuffer::ResetQueryPool(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count)
    {
        vkCmdResetQueryPool(cmdBuffer, std::static_pointer_cast<QueryPool>(queryPool)->GetNativeHandle(), first, count);
    }

    void CommandBuffer::GetQueryResult(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count,
                                       const rhi::BufferPtr &buffer, uint32_t offset, uint32_t stride)
    {
        vkCmdCopyQueryPoolResults(cmdBuffer, std::static_pointer_cast<QueryPool>(queryPool)->GetNativeHandle(),
                                  first, count,
                                  std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle(),
                                  offset, stride,
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    }

    void CommandBuffer::Copy(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy &copy)
    {
        vkCmdCopyImage(cmdBuffer, src, srcLayout, dst, dstLayout, 1, &copy);
    }

    void CommandBuffer::Copy(const BufferPtr &src, const BufferPtr &dst, const VkBufferCopy &copy)
    {
        vkCmdCopyBuffer(cmdBuffer, src->GetNativeHandle(), dst->GetNativeHandle(), 1, &copy);
    }

    ComputeEncoder::ComputeEncoder(CommandBuffer &cb) : cmdBuffer(cb)
    {
        cmd = cmdBuffer.GetNativeHandle();
    }

    void ComputeEncoder::BindShaderResource(const DescriptorSetBinderPtr &binder)
    {
        binder->OnBind(cmd);
    }

    void ComputeEncoder::BindPipeline(const ComputePipelinePtr &pso)
    {
        auto *handle = pso->GetNativeHandle();
        if (handle != currentPso) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, handle);
            currentPso = handle;
        }
    }

    void ComputeEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        vkCmdDispatch(cmd, x, y, z);
    }

    GraphicsEncoder::GraphicsEncoder(CommandBuffer &cb) : cmdBuffer(cb)
    {
        cmd                         = cmdBuffer.GetNativeHandle();
        vkBeginInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkBeginInfo.pNext           = nullptr;
        vkBeginInfo.renderPass      = VK_NULL_HANDLE;
        vkBeginInfo.framebuffer     = VK_NULL_HANDLE;
        vkBeginInfo.renderArea      = {};
        vkBeginInfo.clearValueCount = 0;
        vkBeginInfo.pClearValues    = nullptr;
    }

    void GraphicsEncoder::BeginPass(const PassBeginInfo &beginInfo)
    {
        vkBeginInfo.renderPass = beginInfo.renderPass->GetNativeHandle();
        const auto &extent           = beginInfo.frameBuffer->GetExtent();

        if (beginInfo.renderArea != nullptr) {
            vkBeginInfo.renderArea = *beginInfo.renderArea;
        } else {
            vkBeginInfo.framebuffer         = beginInfo.frameBuffer->GetNativeHandle();
            vkBeginInfo.renderArea.offset.x = 0;
            vkBeginInfo.renderArea.offset.y = 0;
            vkBeginInfo.renderArea.extent   = extent;
        }

        vkBeginInfo.clearValueCount = beginInfo.clearValueCount;
        vkBeginInfo.pClearValues    = beginInfo.clearValues;

        vkCmdBeginRenderPass(cmd, &vkBeginInfo, beginInfo.contents);

        viewport = {0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
        scissor  = {{0, 0}, extent};

        if (beginInfo.contents == VK_SUBPASS_CONTENTS_INLINE) {
            SetViewport(1, &viewport);
            SetScissor(1, &scissor);
        }
    }

    void GraphicsEncoder::BindPipeline(const GraphicsPipelinePtr &pso)
    {
        auto *handle = pso->GetNativeHandle();
        if (handle != currentPso) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, handle);
            currentPso = handle;
        }
    }

    void GraphicsEncoder::Encode(const DrawItem &item)
    {
        if (!item.pso) {
            return;
        }

        if (item.viewport != nullptr && item.viewportCount != 0) {
            SetViewport(item.viewportCount, item.viewport);
        }

        if (item.scissor != nullptr && item.scissorCount != 0) {
            SetScissor(item.scissorCount, item.scissor);
        }

        BindPipeline(item.pso);

        if (item.shaderResources) {
            BindShaderResource(item.shaderResources);
        }

        if (item.vertexAssembly) {
            BindAssembly(item.vertexAssembly);
        }

        if (item.pushConstants) {
            PushConstant(item.pushConstants);
        }

        switch (item.drawArgs.type) {
        case rhi::CmdDrawType::INDEXED:
            DrawIndexed(item.drawArgs.indexed);
            break;
        case rhi::CmdDrawType::LINEAR:
            DrawLinear(item.drawArgs.linear);
            break;
        }
    }

    void GraphicsEncoder::BindAssembly(const VertexAssemblyPtr &assembler)
    {
        if (currentAssembler != assembler) {
            assembler->OnBind(cmd);
            currentAssembler = assembler;
        }
    }

    void GraphicsEncoder::PushConstant(const PushConstantsPtr &constants)
    {
        constants->OnBind(cmd);
    }

    void GraphicsEncoder::SetViewport(uint32_t count, const VkViewport *vp)
    {
        vkCmdSetViewport(cmd, 0, count, vp);
    }

    void GraphicsEncoder::SetScissor(uint32_t count, const VkRect2D *sc)
    {
        vkCmdSetScissor(cmd, 0, count, sc);
    }

    void GraphicsEncoder::BindShaderResource(const DescriptorSetBinderPtr &binder)
    {
        binder->OnBind(cmd);
    }

    void GraphicsEncoder::DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        vkCmdDrawIndirect(cmd, buffer->GetNativeHandle(), offset, size / sizeof(VkDrawIndirectCommand), sizeof(VkDrawIndirectCommand));
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BeginQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        vkCmdBeginQuery(cmd, std::static_pointer_cast<QueryPool>(query)->GetNativeHandle(), id, 0);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        vkCmdEndQuery(cmd, std::static_pointer_cast<QueryPool>(query)->GetNativeHandle(), id);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::WriteTimeStamp(const rhi::QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id)
    {
        vkCmdWriteTimestamp(cmd, FromRHI(stage), std::static_pointer_cast<QueryPool>(query)->GetNativeHandle(), id);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::PassBeginInfo &info)
    {
        auto pass = std::static_pointer_cast<RenderPass>(info.renderPass);
        auto fb = std::static_pointer_cast<FrameBuffer>(info.frameBuffer);
        const auto &attachments = pass->GetAttachments();

        std::vector<VkClearValue> vkClearValue(info.clearCount);
        SKY_ASSERT(attachments.size() == info.clearCount);
        for (uint32_t i = 0; i < info.clearCount; ++i) {
            auto &vkClear = vkClearValue[i];
            auto &clear = info.clearValues[i];
            if (IsDepthStencil(attachments[i].format)) {
                vkClear.depthStencil.depth = clear.depthStencil.depth;
                vkClear.depthStencil.stencil = clear.depthStencil.stencil;
            } else {
                memcpy(vkClear.color.float32, clear.color.float32, 4 * sizeof(float));
            }
        }

        vkBeginInfo.renderPass = pass->GetNativeHandle();
        const auto &extent = fb->GetExtent();

        vkBeginInfo.framebuffer         = fb->GetNativeHandle();
        vkBeginInfo.renderArea.offset.x = 0;
        vkBeginInfo.renderArea.offset.y = 0;
        vkBeginInfo.renderArea.extent   = {extent.width, extent.height};

        vkBeginInfo.clearValueCount = info.clearCount;
        vkBeginInfo.pClearValues    = vkClearValue.data();

        VkSubpassContents contents = info.contents == rhi::SubPassContent::INLINE ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        vkCmdBeginRenderPass(cmd, &vkBeginInfo, contents);

        viewport = {0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
        scissor  = {{0, 0}, extent};

        if (contents == VK_SUBPASS_CONTENTS_INLINE) {
            SetViewport(1, &viewport);
            SetScissor(1, &scissor);
        }

        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        auto vkPso = std::static_pointer_cast<GraphicsPipeline>(pso);
        auto *handle = vkPso->GetNativeHandle();
        if (handle != currentPso) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, handle);
            currentPso = handle;
        }
        currentBinder.SetPipelineLayout(vkPso->GetPipelineLayout());
        currentBinder.SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        std::static_pointer_cast<VertexAssembly>(assembly)->OnBind(cmd);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewports)
    {
        std::vector<VkViewport> vkViewPorts(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto &vkVp    = vkViewPorts[i];
            auto  vp      = viewports[i];
            vkVp.x        = vp.x;
            vkVp.y        = vp.y;
            vkVp.width    = vp.width;
            vkVp.height   = vp.height;
            vkVp.minDepth = vp.minDepth;
            vkVp.maxDepth = vp.maxDepth;
        }
        vkCmdSetViewport(cmd, 0, count, vkViewPorts.data());
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissors)
    {
        std::vector<VkRect2D> vkScissors(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto &vkSc         = vkScissors[i];
            auto  sc           = scissors[i];
            vkSc.offset.x      = sc.offset.x;
            vkSc.offset.y      = sc.offset.y;
            vkSc.extent.width  = sc.extent.width;
            vkSc.extent.height = sc.extent.height;
        }
        vkCmdSetScissor(cmd, 0, count, vkScissors.data());
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        vkCmdDrawIndexed(cmd, indexed.indexCount, indexed.instanceCount, indexed.firstIndex, indexed.vertexOffset, indexed.firstInstance);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        currentBinder.OnBind(cmd);
        vkCmdDraw(cmd, linear.vertexCount, linear.instanceCount, linear.firstVertex, linear.firstInstance);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexedIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        currentBinder.OnBind(cmd);

        if (cmdBuffer.GetDevice().GetFeatures().multiDrawIndirect) {
            vkCmdDrawIndexedIndirect(cmd, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle(), offset, count, stride);
        } else {
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t off = offset + i * stride;
                vkCmdDrawIndexedIndirect(cmd, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle(), off, 1, stride);
            }
        }

        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        currentBinder.OnBind(cmd);

        if (cmdBuffer.GetDevice().GetFeatures().multiDrawIndirect) {
            vkCmdDrawIndirect(cmd, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle(), offset, count, stride);
        } else {
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t off = offset + i * stride;
                vkCmdDrawIndirect(cmd, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle(), off, 1, stride);
            }
        }
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindSet(uint32_t id, const rhi::DescriptorSetPtr &set)
    {
        currentBinder.BindSet(id, std::static_pointer_cast<DescriptorSet>(set));
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::NextSubPass()
    {
        vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        vkCmdEndRenderPass(cmd);
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyTexture()
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyTextureToBuffer()
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyBufferToTexture()
    {
        return *this;
    }

    /**
     * for copy and blit command:
     * src layout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * dst layout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     *
     * use fixed VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
     */

    rhi::BlitEncoder &BlitEncoder::BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter)
    {
        blit.resize(blitInputs.size());

        for (uint32_t i = 0; i < blitInputs.size(); ++i) {
            auto &VkBlit = blit[i];
            const auto &rBlit = blitInputs[i];

            VkBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
            VkBlit.pNext = nullptr;
            VkBlit.srcSubresource = FromRHI(rBlit.srcRange);
            VkBlit.dstSubresource = FromRHI(rBlit.dstRange);
            VkBlit.srcOffsets[0]  = FromRHI(rBlit.srcOffsets[0]);
            VkBlit.srcOffsets[1]  = FromRHI(rBlit.srcOffsets[1]);
            VkBlit.dstOffsets[0]  = FromRHI(rBlit.dstOffsets[0]);
            VkBlit.dstOffsets[1]  = FromRHI(rBlit.dstOffsets[1]);
        }

        blitInfo.srcImage = std::static_pointer_cast<Image>(src)->GetNativeHandle();
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = std::static_pointer_cast<Image>(dst)->GetNativeHandle();
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = static_cast<uint32_t>(blit.size());
        blitInfo.pRegions = blit.data();
        blitInfo.filter = FromRHI(filter);

        CmdBlitImage2(cmdBuffer.GetNativeHandle(), &blitInfo);
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::ResolveInfo> &resolveInputs)
    {
        resolves.resize(resolveInputs.size());
        for (uint32_t i = 0; i < resolveInputs.size(); ++i) {
            auto &vkResolve = resolves[i];
            const auto &rResolve = resolveInputs[i];

            vkResolve.sType = VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2;
            vkResolve.pNext = nullptr;
            vkResolve.srcSubresource = FromRHI(rResolve.srcRange);
            vkResolve.srcOffset      = FromRHI(rResolve.srcOffset);
            vkResolve.dstSubresource = FromRHI(rResolve.dstRange);
            vkResolve.dstOffset      = FromRHI(rResolve.dstOffset);
            vkResolve.extent         = FromRHI(rResolve.extent);
        }

        resolveInfo.srcImage = std::static_pointer_cast<Image>(src)->GetNativeHandle();
        resolveInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        resolveInfo.dstImage = std::static_pointer_cast<Image>(dst)->GetNativeHandle();
        resolveInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        resolveInfo.regionCount = static_cast<uint32_t>(resolves.size());
        resolveInfo.pRegions = resolves.data();
        CmdResolveImage2(cmdBuffer.GetNativeHandle(), &resolveInfo);
        return *this;
    }

    void SecondaryCommands::Emplace(const CommandBufferPtr &cmd)
    {
        std::lock_guard<std::mutex> lock(mutex);
        cmdBuffers.emplace_back(cmd);
        cmdHandlers.emplace_back(cmd->GetNativeHandle());
    }

    void SecondaryCommands::OnExecute(VkCommandBuffer main) const
    {
        vkCmdExecuteCommands(main, static_cast<uint32_t>(cmdHandlers.size()), cmdHandlers.data());
    }
} // namespace sky::vk
