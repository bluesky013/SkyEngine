//
// Created by Zach Lee on 2021/12/26.
//

#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/Image.h>
#include <vulkan/ImageView.h>

namespace sky::drv {

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
        res.depthStencil.depth = depth;
        res.depthStencil.stencil = stencil;
        return res;
    }

    inline Image::Descriptor MakeImage2D(VkFormat format, const VkExtent2D& ext, uint32_t layers = 1, uint32_t levels = 1)
    {
        Image::Descriptor des;
        des.imageType = VK_IMAGE_TYPE_2D;
        des.format = format;
        des.extent.width = ext.width;
        des.extent.height = ext.height;
        des.extent.depth = 1;
        return des;
    }

    inline bool IsBufferDescriptor(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
            type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }

    inline bool IsImageDescriptor(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ||
            type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
}
