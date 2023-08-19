//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

namespace sky::vk {

    inline VkExtent3D FromRHI(const rhi::Extent3D &ext) { return {ext.width, ext.height, ext.depth}; }
    inline VkOffset3D FromRHI(const rhi::Offset3D &off) { return {off.x, off.y, off.z}; }
    VkImageType FromRHI(rhi::ImageType type);
    VkImageViewType FromRHI(rhi::ImageViewType type);
    VkImageAspectFlags FromRHI(const rhi::AspectFlags& flags);
    VkFormat FromRHI(rhi::PixelFormat format);
    VkFormat FromRHI(rhi::Format format);
    VkVertexInputRate FromRHI(rhi::VertexInputRate rate);
    VmaMemoryUsage FromRHI(rhi::MemoryType type, const rhi::ImageUsageFlags& flags = rhi::ImageUsageFlags{});
    VkImageUsageFlags FromRHI(const rhi::ImageUsageFlags&);
    VkBufferUsageFlags FromRHI(const rhi::BufferUsageFlags&);
    VkFilter FromRHI(rhi::Filter);
    VkSamplerMipmapMode FromRHI(rhi::MipFilter);
    VkSamplerAddressMode FromRHI(rhi::WrapMode mode);
    VkShaderStageFlags FromRHI(const rhi::ShaderStageFlags&);
    VkPipelineStageFlags FromRHI(const rhi::PipelineStageFlags&);
    VkPipelineStageFlagBits FromRHI(const rhi::PipelineStageBit&);
    VkAccessFlags FromRHI(rhi::AccessFlags);
    VkSampleCountFlagBits FromRHI(rhi::SampleCount);
    VkAttachmentLoadOp FromRHI(rhi::LoadOp);
    VkAttachmentStoreOp FromRHI(rhi::StoreOp);
    VkCompareOp FromRHI(rhi::CompareOp);
    VkPrimitiveTopology FromRHI(rhi::PrimitiveTopology);
    VkPolygonMode FromRHI(rhi::PolygonMode);
    VkCullModeFlags FromRHI(const rhi::CullingModeFlags&);
    VkFrontFace FromRHI(rhi::FrontFace);
    VkBlendFactor FromRHI(rhi::BlendFactor);
    VkBlendOp FromRHI(rhi::BlendOp);
    VkStencilOp FromRHI(rhi::StencilOp);
    VkIndexType FromRHI(rhi::IndexType);
    VkStencilOpState FromRHI(const rhi::StencilState&);
    VkDescriptorType FromRHI(rhi::DescriptorType);
    VkImageSubresourceLayers FromRHI(const rhi::ImageSubRangeLayers &res);
    VkQueryType FromRHI(rhi::QueryType);
    VkQueryPipelineStatisticFlags FromRHI(const rhi::PipelineStatisticFlags& flags);
}