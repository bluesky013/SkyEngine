//
// Created on 2026/04/07.
//

#include <VulkanEncoder.h>
#include <VulkanDevice.h>
#include <VulkanBuffer.h>
#include <VulkanImage.h>
#include <VulkanPipelineState.h>
#include <VulkanConversion.h>

namespace sky::aurora {

    static VkImageAspectFlags InferAspectMask(VkFormat format)
    {
        switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    static void FixImageSubresourceLayers(VkImageSubresourceLayers &subresource, VkFormat format)
    {
        if (subresource.aspectMask == 0) {
            subresource.aspectMask = InferAspectMask(format);
        }
    }

    // ---- VulkanGraphicsEncoder ----

    VulkanGraphicsEncoder::VulkanGraphicsEncoder(VulkanDevice &device, VkCommandBuffer cmd)
        : fn(device.GetDeviceFn())
        , cmd(cmd)
    {
    }

    void VulkanGraphicsEncoder::BeginRendering(const RenderingInfo &info)
    {
        VkRenderingAttachmentInfo colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
        for (uint32_t i = 0; i < info.numColors; ++i) {
            auto &src = info.colors[i];
            auto &dst = colorAttachments[i];
            auto *image = static_cast<VulkanImage *>(src.image);
            dst.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            dst.imageView   = image != nullptr ? image->GetDefaultView() : VK_NULL_HANDLE;
            dst.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            dst.loadOp      = FromLoadOp(src.loadOp);
            dst.storeOp     = FromStoreOp(src.storeOp);
            dst.clearValue.color = {{
                src.clearValue.color.float32[0],
                src.clearValue.color.float32[1],
                src.clearValue.color.float32[2],
                src.clearValue.color.float32[3]
            }};
        }

        VkRenderingAttachmentInfo depthAttachment = {};
        auto *depthImage = static_cast<VulkanImage *>(info.depthStencil.image);
        depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView   = depthImage != nullptr ? depthImage->GetDefaultView() : VK_NULL_HANDLE;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp      = FromLoadOp(info.depthStencil.depthLoadOp);
        depthAttachment.storeOp     = FromStoreOp(info.depthStencil.depthStoreOp);
        depthAttachment.clearValue.depthStencil = {
            info.depthStencil.clearValue.depthStencil.depth,
            info.depthStencil.clearValue.depthStencil.stencil
        };

        VkRenderingInfo renderInfo = {};
        renderInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea.offset    = {info.renderArea.offset.x, info.renderArea.offset.y};
        renderInfo.renderArea.extent    = {info.renderArea.extent.width, info.renderArea.extent.height};
        renderInfo.layerCount           = 1;
        renderInfo.colorAttachmentCount = info.numColors;
        renderInfo.pColorAttachments    = info.numColors > 0 ? colorAttachments : nullptr;
        if (info.depthStencil.image != nullptr) {
            renderInfo.pDepthAttachment = &depthAttachment;
        }

        fn.vkCmdBeginRendering(cmd, &renderInfo);
    }

    void VulkanGraphicsEncoder::EndRendering()
    {
        fn.vkCmdEndRendering(cmd);
    }

    void VulkanGraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto *vkPso = static_cast<VulkanGraphicsPipeline *>(pso);
        fn.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPso->GetNativeHandle());
    }

    void VulkanGraphicsEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup has VkDescriptorSet
    }

    void VulkanGraphicsEncoder::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views)
    {
        constexpr uint32_t MAX_VB = 16;
        VkBuffer     buffers[MAX_VB];
        VkDeviceSize offsets[MAX_VB];

        uint32_t n = count < MAX_VB ? count : MAX_VB;
        for (uint32_t i = 0; i < n; ++i) {
            buffers[i] = static_cast<VulkanBuffer *>(views[i].buffer)->GetNativeHandle();
            offsets[i] = views[i].offset;
        }
        fn.vkCmdBindVertexBuffers(cmd, firstBinding, n, buffers, offsets);
    }

    void VulkanGraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        auto *vkBuf = static_cast<VulkanBuffer *>(buffer);
        fn.vkCmdBindIndexBuffer(cmd, vkBuf->GetNativeHandle(), offset, FromIndexType(type));
    }

    void VulkanGraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        VkViewport vkViewports[16];
        uint32_t n = count < 16 ? count : 16;
        for (uint32_t i = 0; i < n; ++i) {
            vkViewports[i].x        = viewports[i].x;
            vkViewports[i].y        = viewports[i].y;
            vkViewports[i].width    = viewports[i].width;
            vkViewports[i].height   = viewports[i].height;
            vkViewports[i].minDepth = viewports[i].minDepth;
            vkViewports[i].maxDepth = viewports[i].maxDepth;
        }
        fn.vkCmdSetViewport(cmd, 0, n, vkViewports);
    }

    void VulkanGraphicsEncoder::SetScissor(uint32_t count, const Rect2D *scissors)
    {
        VkRect2D vkScissors[16];
        uint32_t n = count < 16 ? count : 16;
        for (uint32_t i = 0; i < n; ++i) {
            vkScissors[i].offset = {scissors[i].offset.x, scissors[i].offset.y};
            vkScissors[i].extent = {scissors[i].extent.width, scissors[i].extent.height};
        }
        fn.vkCmdSetScissor(cmd, 0, n, vkScissors);
    }

    void VulkanGraphicsEncoder::Draw(const CmdDrawLinear &draw)
    {
        fn.vkCmdDraw(cmd, draw.vertexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
    }

    void VulkanGraphicsEncoder::DrawIndexed(const CmdDrawIndexed &draw)
    {
        fn.vkCmdDrawIndexed(cmd, draw.indexCount, draw.instanceCount, draw.firstIndex, draw.vertexOffset, draw.firstInstance);
    }

    void VulkanGraphicsEncoder::DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        auto *vkBuf = static_cast<VulkanBuffer *>(buffer);
        fn.vkCmdDrawIndirect(cmd, vkBuf->GetNativeHandle(), offset, drawCount, stride);
    }

    void VulkanGraphicsEncoder::DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        auto *vkBuf = static_cast<VulkanBuffer *>(buffer);
        fn.vkCmdDrawIndexedIndirect(cmd, vkBuf->GetNativeHandle(), offset, drawCount, stride);
    }

    // ---- VulkanComputeEncoder ----

    VulkanComputeEncoder::VulkanComputeEncoder(VulkanDevice &device, VkCommandBuffer cmd)
        : fn(device.GetDeviceFn())
        , cmd(cmd)
    {
    }

    void VulkanComputeEncoder::BindPipeline(ComputePipeline *pso)
    {
        auto *vkPso = static_cast<VulkanComputePipeline *>(pso);
        fn.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPso->GetNativeHandle());
    }

    void VulkanComputeEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup has VkDescriptorSet
    }

    void VulkanComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        fn.vkCmdDispatch(cmd, groupX, groupY, groupZ);
    }

    void VulkanComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        auto *vkBuf = static_cast<VulkanBuffer *>(buffer);
        fn.vkCmdDispatchIndirect(cmd, vkBuf->GetNativeHandle(), offset);
    }

    // ---- VulkanBlitEncoder ----

    VulkanBlitEncoder::VulkanBlitEncoder(VulkanDevice &device, VkCommandBuffer cmd)
        : fn(device.GetDeviceFn())
        , cmd(cmd)
    {
    }

    void VulkanBlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        VkBufferCopy region = {};
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;
        region.size      = size;
        fn.vkCmdCopyBuffer(cmd,
            static_cast<VulkanBuffer *>(src)->GetNativeHandle(),
            static_cast<VulkanBuffer *>(dst)->GetNativeHandle(),
            1, &region);
    }

    void VulkanBlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        const auto dstFormat = static_cast<VulkanImage *>(dst)->GetVkFormat();
        std::vector<VkBufferImageCopy> vkRegions(regions.size());
        for (size_t i = 0; i < regions.size(); ++i) {
            auto &s = regions[i];
            auto &d = vkRegions[i];
            d.bufferOffset      = s.bufferOffset;
            d.bufferRowLength   = s.bufferRowLength;
            d.bufferImageHeight = s.bufferImageHeight;
            d.imageSubresource  = FromImageSubRangeLayers(s.subRange);
            FixImageSubresourceLayers(d.imageSubresource, dstFormat);
            d.imageOffset       = {s.imageOffset.x, s.imageOffset.y, s.imageOffset.z};
            d.imageExtent       = {s.imageExtent.width, s.imageExtent.height, s.imageExtent.depth};
        }
        fn.vkCmdCopyBufferToImage(cmd,
            static_cast<VulkanBuffer *>(src)->GetNativeHandle(),
            static_cast<VulkanImage *>(dst)->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data());
    }

    void VulkanBlitEncoder::CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions)
    {
        const auto srcFormat = static_cast<VulkanImage *>(src)->GetVkFormat();
        std::vector<VkBufferImageCopy> vkRegions(regions.size());
        for (size_t i = 0; i < regions.size(); ++i) {
            auto &s = regions[i];
            auto &d = vkRegions[i];
            d.bufferOffset      = s.bufferOffset;
            d.bufferRowLength   = s.bufferRowLength;
            d.bufferImageHeight = s.bufferImageHeight;
            d.imageSubresource  = FromImageSubRangeLayers(s.subRange);
            FixImageSubresourceLayers(d.imageSubresource, srcFormat);
            d.imageOffset       = {s.imageOffset.x, s.imageOffset.y, s.imageOffset.z};
            d.imageExtent       = {s.imageExtent.width, s.imageExtent.height, s.imageExtent.depth};
        }
        fn.vkCmdCopyImageToBuffer(cmd,
            static_cast<VulkanImage *>(src)->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<VulkanBuffer *>(dst)->GetNativeHandle(),
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data());
    }

    void VulkanBlitEncoder::BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter)
    {
        const auto srcFormat = static_cast<VulkanImage *>(src)->GetVkFormat();
        const auto dstFormat = static_cast<VulkanImage *>(dst)->GetVkFormat();
        std::vector<VkImageBlit> vkRegions(regions.size());
        for (size_t i = 0; i < regions.size(); ++i) {
            auto &s = regions[i];
            auto &d = vkRegions[i];
            d.srcSubresource = FromImageSubRangeLayers(s.srcRange);
            d.dstSubresource = FromImageSubRangeLayers(s.dstRange);
            FixImageSubresourceLayers(d.srcSubresource, srcFormat);
            FixImageSubresourceLayers(d.dstSubresource, dstFormat);
            d.srcOffsets[0]  = {s.srcOffsets[0].x, s.srcOffsets[0].y, s.srcOffsets[0].z};
            d.srcOffsets[1]  = {s.srcOffsets[1].x, s.srcOffsets[1].y, s.srcOffsets[1].z};
            d.dstOffsets[0]  = {s.dstOffsets[0].x, s.dstOffsets[0].y, s.dstOffsets[0].z};
            d.dstOffsets[1]  = {s.dstOffsets[1].x, s.dstOffsets[1].y, s.dstOffsets[1].z};
        }
        fn.vkCmdBlitImage(cmd,
            static_cast<VulkanImage *>(src)->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<VulkanImage *>(dst)->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data(),
            FromFilter(filter));
    }

    void VulkanBlitEncoder::ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions)
    {
        const auto srcFormat = static_cast<VulkanImage *>(src)->GetVkFormat();
        const auto dstFormat = static_cast<VulkanImage *>(dst)->GetVkFormat();
        std::vector<VkImageResolve> vkRegions(regions.size());
        for (size_t i = 0; i < regions.size(); ++i) {
            auto &s = regions[i];
            auto &d = vkRegions[i];
            d.srcSubresource = FromImageSubRangeLayers(s.srcRange);
            d.dstSubresource = FromImageSubRangeLayers(s.dstRange);
            FixImageSubresourceLayers(d.srcSubresource, srcFormat);
            FixImageSubresourceLayers(d.dstSubresource, dstFormat);
            d.srcOffset      = {s.srcOffset.x, s.srcOffset.y, s.srcOffset.z};
            d.dstOffset      = {s.dstOffset.x, s.dstOffset.y, s.dstOffset.z};
            d.extent         = {s.extent.width, s.extent.height, s.extent.depth};
        }
        fn.vkCmdResolveImage(cmd,
            static_cast<VulkanImage *>(src)->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<VulkanImage *>(dst)->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data());
    }

} // namespace sky::aurora
