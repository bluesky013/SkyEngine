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

    struct BufferUploadRequest {
        const uint8_t *data;
        uint64_t       offset;
        uint64_t       size;
    };

    struct ImageUploadRequest {
        const uint8_t *data;
        uint64_t       offset;
        uint32_t       mipLevel;
        uint32_t       layer;
    };

    struct ImageFormatInfo {
        uint32_t blockSize;
        uint32_t blockWidth;
        uint32_t blockHeight;
        bool isCompressed;
    };

} // namespace sky::vk
