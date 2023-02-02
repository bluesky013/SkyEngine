//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/CommandBuffer.h"
#include "core/logger/Logger.h"
#include "vulkan/Device.h"
#include "vulkan/Fence.h"

static const char *TAG = "Vulkan";

namespace sky::vk {

    CommandBuffer::CommandBuffer(Device &dev) : DevObject(dev), pool(VK_NULL_HANDLE), cmdBuffer(VK_NULL_HANDLE), fence()
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        Wait();

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
        if (des.needFence) {
            Fence::VkDescriptor fenceDes = {};
            fenceDes.flag              = VK_FENCE_CREATE_SIGNALED_BIT;
            fence                      = device.CreateDeviceObject<Fence>(fenceDes);
            if (!fence) {
                return false;
            }
        }

        return true;
    }

    void CommandBuffer::Wait()
    {
        if (fence) {
            fence->Wait();
        }
    }

    void CommandBuffer::Begin()
    {
        if (fence) {
            fence->Reset();
        }

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

    void CommandBuffer::BeginQuery(const QueryPoolPtr &pool, uint32_t queryId)
    {
        vkCmdBeginQuery(cmdBuffer, pool->GetNativeHandle(), queryId, 0);
    }

    void CommandBuffer::EndQuery(const QueryPoolPtr &pool, uint32_t queryId)
    {
        vkCmdEndQuery(cmdBuffer, pool->GetNativeHandle(), queryId);
    }

    void CommandBuffer::ResetQueryPool(const QueryPoolPtr &pool, uint32_t first, uint32_t count)
    {
        vkCmdResetQueryPool(cmdBuffer, pool->GetNativeHandle(), first, count);
    }

    void CommandBuffer::End()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    void CommandBuffer::Submit(rhi::Queue &queue)
    {
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
        vkQueueSubmit(queue.GetNativeHandle(), 1, &submitInfo, fence ? fence->GetNativeHandle() : VK_NULL_HANDLE);
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle() const
    {
        return cmdBuffer;
    }

    GraphicsEncoder CommandBuffer::EncodeVkGraphics()
    {
        return GraphicsEncoder(*this);
    }

    void CommandBuffer::Copy(const BufferPtr &src, const ImagePtr &dst, const VkBufferImageCopy &copy)
    {
        vkCmdCopyBufferToImage(cmdBuffer, src->GetNativeHandle(), dst->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    void CommandBuffer::ImageBarrier(
        const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst)
    {
        QueueBarrier(image, subresourceRange, barrier, src, dst);
        FlushBarrier();
    }

    void CommandBuffer::BufferBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset)
    {
        QueueBarrier(buffer, barrier, size, offset);
        FlushBarrier();
    }

    void CommandBuffer::QueueBarrier(const ImagePtr &image, const VkImageSubresourceRange &subresourceRange, const Barrier &barrier, VkImageLayout src, VkImageLayout dst)
    {
        imageBarriers.emplace_back(VkImageMemoryBarrier2{});
        auto &imageMemoryBarrier = imageBarriers.back();
        imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageMemoryBarrier.srcStageMask         = barrier.srcStageMask;
        imageMemoryBarrier.srcAccessMask        = barrier.srcAccessMask;
        imageMemoryBarrier.dstStageMask         = barrier.dstStageMask;
        imageMemoryBarrier.dstAccessMask        = barrier.dstAccessMask;
        imageMemoryBarrier.image                = image->GetNativeHandle();
        imageMemoryBarrier.subresourceRange     = subresourceRange;
        imageMemoryBarrier.oldLayout            = src;
        imageMemoryBarrier.newLayout            = dst;
        imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    }

    void CommandBuffer::QueueBarrier(const BufferPtr &buffer, const Barrier &barrier, VkDeviceSize size, VkDeviceSize offset)
    {
        bufferBarriers.emplace_back(VkBufferMemoryBarrier2{});
        auto &bufferBarrier = bufferBarriers.back();
        bufferBarrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        bufferBarrier.srcStageMask          = barrier.srcStageMask;
        bufferBarrier.srcAccessMask         = barrier.srcAccessMask;
        bufferBarrier.dstStageMask          = barrier.dstStageMask;
        bufferBarrier.dstAccessMask         = barrier.dstAccessMask;
        bufferBarrier.buffer                = buffer->GetNativeHandle();
        bufferBarrier.offset                = offset;
        bufferBarrier.size                  = size;
        bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    }

    void CommandBuffer::FlushBarrier()
    {
        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
        dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
        dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
        dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
        vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        bufferBarriers.clear();
        imageBarriers.clear();
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
    }

    void ComputeEncoder::BindShaderResource(const DescriptorSetBinderPtr &binder)
    {
        binder->OnBind(cmd);
    }

    void ComputeEncoder::BindComputePipeline(const ComputePipelinePtr &pso)
    {
        auto handle = pso->GetNativeHandle();
        if (handle != currentPso) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, handle);
            currentPso = handle;
        }
    }

    void ComputeEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        vkCmdDispatch(cmd, x, y, z);
    }

    GraphicsEncoder::GraphicsEncoder(CommandBuffer &cb) : ComputeEncoder{cb}
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
        auto &extent           = beginInfo.frameBuffer->GetExtent();

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

    void GraphicsEncoder::EndPass()
    {
        vkCmdEndRenderPass(cmd);
    }

    void GraphicsEncoder::BindPipeline(const GraphicsPipelinePtr &pso)
    {
        auto handle = pso->GetNativeHandle();
        if (handle != currentPso) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, handle);
            currentPso = handle;
        }
    }

    const VkRenderPassBeginInfo &GraphicsEncoder::GetCurrentPass() const
    {
        return vkBeginInfo;
    }

    uint32_t GraphicsEncoder::GetSubPassId() const
    {
        return currentSubPassId;
    }

    const VkViewport &GraphicsEncoder::GetCurrentViewport() const
    {
        return viewport;
    }

    const VkRect2D &GraphicsEncoder::GetCurrentScissor() const
    {
        return scissor;
    }

    CommandBuffer &GraphicsEncoder::GetCommandBuffer()
    {
        return cmdBuffer;
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

    void GraphicsEncoder::SetViewport(uint32_t count, const VkViewport *viewport)
    {
        vkCmdSetViewport(cmd, 0, count, viewport);
    }

    void GraphicsEncoder::SetScissor(uint32_t count, const VkRect2D *scissor)
    {
        vkCmdSetScissor(cmd, 0, count, scissor);
    }

    void GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        vkCmdDrawIndexed(cmd, indexed.indexCount, indexed.instanceCount, indexed.firstIndex, indexed.vertexOffset, indexed.firstInstance);
    }

    void GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        vkCmdDraw(cmd, linear.vertexCount, linear.instanceCount, linear.firstVertex, linear.firstInstance);
    }

    void GraphicsEncoder::DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        vkCmdDrawIndirect(cmd, buffer->GetNativeHandle(), offset, size / sizeof(VkDrawIndirectCommand), sizeof(VkDrawIndirectCommand));
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
