//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

namespace sky::vk {

    inline VkExtent3D FromRHI(const rhi::Extent3D &ext)
    {
        return {ext.width, ext.height, ext.depth};
    }

    VkImageType FromRHI(rhi::ImageType type);
    VkImageViewType FromRHI(rhi::ImageViewType type);
    VkImageAspectFlags FromRHI(rhi::AspectFlags flags);
    VkFormat FromRHI(rhi::PixelFormat format);
    VmaMemoryUsage FromRHI(rhi::MemoryType type);
    VkImageUsageFlags FromRHI(rhi::ImageUsageFlags);
    VkBufferUsageFlags FromRHI(rhi::BufferUsageFlags);
    VkFilter FromRHI(rhi::Filter);
    VkSamplerMipmapMode FromRHI(rhi::MipFilter);
    VkSamplerAddressMode FromRHI(rhi::WrapMode mode);
    VkShaderStageFlags FromRHI(rhi::ShaderStageFlags);
}