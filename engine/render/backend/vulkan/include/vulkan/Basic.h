//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/vulkan.h>
#define VKL_ALLOC nullptr

#include <vector>
#include <algorithm>
#include <cstring>

namespace sky::vk {

    struct Barrier {
        VkPipelineStageFlags srcStageMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags        srcAccessMask;
        VkAccessFlags        dstAccessMask;
    };

    struct AccessInfo {
        VkPipelineStageFlags pipelineStages;
        VkAccessFlags accessFlags;
        VkImageLayout imageLayout;
    };

    struct MemoryRequirement {
        VkDeviceSize size;
        VkDeviceSize alignment;
        uint32_t     memoryIndex;
        bool prefersDedicated;
        bool requiresDedicated;
    };

    inline bool CheckExtension(const std::vector<VkExtensionProperties> &extensions, const char* ext)
    {
        return std::any_of(extensions.begin(), extensions.end(), [ext](const VkExtensionProperties &prop) {
            return strcmp(prop.extensionName, ext) == 0;
        });
    }
} // namespace sky::vk
