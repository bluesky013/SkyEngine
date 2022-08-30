//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/vulkan.h>
#define VKL_ALLOC nullptr

namespace sky::drv {

    struct Barrier {
        VkPipelineStageFlags srcStageMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags        srcAccessMask;
        VkAccessFlags        dstAccessMask;
    };

} // namespace sky::drv