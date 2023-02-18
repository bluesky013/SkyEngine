//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/vulkan.h>
#define VKL_ALLOC nullptr

namespace sky::vk {

    struct Barrier {
        VkPipelineStageFlags srcStageMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags        srcAccessMask;
        VkAccessFlags        dstAccessMask;
    };

    struct MemoryRequirement {
        VkDeviceSize size;
        VkDeviceSize alignment;
        uint32_t     memoryIndex;
        bool prefersDedicated;
        bool requiresDedicated;
    };

} // namespace sky::vk
