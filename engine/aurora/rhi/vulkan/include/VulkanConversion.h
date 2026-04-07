//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <vk_mem_alloc.h>

namespace sky::aurora {

    // pixel / vertex format
    VkFormat              FromPixelFormat(PixelFormat format);
    VkFormat              FromFormat(Format format);
    VkSampleCountFlagBits FromSampleCount(SampleCount sample);

    // image
    VkImageType           FromImageType(ImageType type);
    VkImageViewType       FromImageViewType(ImageViewType type);
    VkImageUsageFlags     FromImageUsageFlags(const ImageUsageFlags &flags);
    VkImageAspectFlags    FromAspectFlags(const AspectFlags &flags);

    // buffer
    VkBufferUsageFlags    FromBufferUsageFlags(const BufferUsageFlags &flags);

    // memory
    VmaMemoryUsage        FromMemoryType(MemoryType type);
    VmaMemoryUsage        FromMemoryType(MemoryType type, const ImageUsageFlags &usage);

    // sampler
    VkFilter              FromFilter(Filter filter);
    VkSamplerMipmapMode   FromMipFilter(MipFilter filter);
    VkSamplerAddressMode  FromWrapMode(WrapMode mode);

    // pipeline state
    VkBlendFactor         FromBlendFactor(BlendFactor factor);
    VkBlendOp             FromBlendOp(BlendOp op);
    VkCompareOp           FromCompareOp(CompareOp op);
    VkStencilOp           FromStencilOp(StencilOp op);
    VkPrimitiveTopology   FromPrimitiveTopology(PrimitiveTopology topo);
    VkPolygonMode         FromPolygonMode(PolygonMode mode);
    VkFrontFace           FromFrontFace(FrontFace face);
    VkCullModeFlags       FromCullMode(const CullingModeFlags &flags);
    VkIndexType           FromIndexType(IndexType type);
    VkStencilOpState      FromStencilState(const StencilState &state);

    // render pass
    VkAttachmentLoadOp    FromLoadOp(LoadOp op);
    VkAttachmentStoreOp   FromStoreOp(StoreOp op);

    // descriptor
    VkDescriptorType         FromDescriptorType(DescriptorType type);
    VkDescriptorBindingFlags FromDescriptorBindingFlags(const DescriptorBindingFlags &flags);

    // shader / vertex input
    VkVertexInputRate        FromVertexInputRate(VertexInputRate rate);
    VkShaderStageFlagBits    FromShaderStage(ShaderStageFlagBit stage);
    VkShaderStageFlags       FromShaderStageFlags(const ShaderStageFlags &flags);

    // synchronization
    VkPipelineStageFlags     FromPipelineStageFlags(const PipelineStageFlags &flags);

    // query
    VkQueryType                    FromQueryType(QueryType type);
    VkQueryPipelineStatisticFlags  FromPipelineStatisticFlags(const PipelineStatisticFlags &flags);

    // subresource
    VkImageSubresourceLayers FromImageSubRangeLayers(const ImageSubRangeLayers &range);

} // namespace sky::aurora
