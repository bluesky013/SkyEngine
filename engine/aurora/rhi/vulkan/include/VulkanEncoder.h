//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/Encoder.h>
#include <VulkanFunctions.h>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanCommandBuffer;

    class VulkanGraphicsEncoder : public GraphicsEncoder {
    public:
        VulkanGraphicsEncoder(VulkanDevice &device, VkCommandBuffer cmd);
        ~VulkanGraphicsEncoder() override = default;

        void BeginRendering(const RenderingInfo &info) override;
        void EndRendering() override;

        void BindPipeline(GraphicsPipeline *pso) override;
        void BindResourceGroup(uint32_t set, ResourceGroup *group) override;
        void BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views) override;
        void BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type) override;

        void SetViewport(uint32_t count, const Viewport *viewports) override;
        void SetScissor(uint32_t count, const Rect2D *scissors) override;

        void Draw(const CmdDrawLinear &cmd) override;
        void DrawIndexed(const CmdDrawIndexed &cmd) override;
        void DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
        void DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    private:
        const VulkanDeviceFunctions &fn;
        VkCommandBuffer              cmd = VK_NULL_HANDLE;
    };

    class VulkanComputeEncoder : public ComputeEncoder {
    public:
        VulkanComputeEncoder(VulkanDevice &device, VkCommandBuffer cmd);
        ~VulkanComputeEncoder() override = default;

        void BindPipeline(ComputePipeline *pso) override;
        void BindResourceGroup(uint32_t set, ResourceGroup *group) override;
        void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;
        void DispatchIndirect(Buffer *buffer, uint64_t offset) override;

    private:
        const VulkanDeviceFunctions &fn;
        VkCommandBuffer              cmd = VK_NULL_HANDLE;
    };

    class VulkanBlitEncoder : public BlitEncoder {
    public:
        VulkanBlitEncoder(VulkanDevice &device, VkCommandBuffer cmd);
        ~VulkanBlitEncoder() override = default;

        void CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) override;
        void CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions) override;
        void CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions) override;
        void BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter) override;
        void ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions) override;

    private:
        const VulkanDeviceFunctions &fn;
        VkCommandBuffer              cmd = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
