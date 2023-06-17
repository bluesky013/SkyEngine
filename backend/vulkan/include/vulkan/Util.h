//
// Created by Zach Lee on 2021/12/26.
//

#pragma once
#include <vulkan/DrawItem.h>
#include <vulkan/Image.h>
#include <vulkan/ImageView.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    inline VkClearValue MakeClearColor(float r, float g, float b, float a)
    {
        VkClearValue res;
        res.color.float32[0] = r;
        res.color.float32[1] = g;
        res.color.float32[2] = b;
        res.color.float32[3] = a;
        return res;
    }

    inline VkClearValue MakeClearDepthStencil(float depth, uint32_t stencil)
    {
        VkClearValue res;
        res.depthStencil.depth   = depth;
        res.depthStencil.stencil = stencil;
        return res;
    }

    inline bool IsBufferDescriptor(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
               type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }

    inline bool IsImageDescriptor(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
               type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }

    inline bool IsDynamicDescriptor(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }

} // namespace sky::vk
