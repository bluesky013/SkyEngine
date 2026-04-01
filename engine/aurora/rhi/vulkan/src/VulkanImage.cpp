//
// Created on 2026/04/02.
//

#include "VulkanImage.h"
#include "VulkanDevice.h"
#include "VulkanConversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanImage::VulkanImage(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanImage::~VulkanImage()
    {
        const auto vkDevice = device.GetNativeHandle();
        if (owned && image != VK_NULL_HANDLE) {
            vkDestroyImage(vkDevice, image, nullptr);
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(vkDevice, memory, nullptr);
        }
    }

    bool VulkanImage::Init(const Descriptor &desc)
    {
        vkFormat = FromPixelFormat(desc.format);
        if (vkFormat == VK_FORMAT_UNDEFINED) {
            LOG_E(TAG, "unsupported pixel format for image");
            return false;
        }

        VkImageCreateInfo imageCI = {};
        imageCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType     = FromImageType(desc.imageType);
        imageCI.format        = vkFormat;
        imageCI.extent.width  = desc.extent.width;
        imageCI.extent.height = desc.extent.height;
        imageCI.extent.depth  = desc.extent.depth;
        imageCI.mipLevels     = desc.mipLevels;
        imageCI.arrayLayers   = desc.arrayLayers;
        imageCI.samples       = FromSampleCount(desc.samples);
        imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage         = FromImageUsageFlags(desc.usage);
        imageCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (desc.viewUsage & ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE) {
            imageCI.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        const auto vkDevice = device.GetNativeHandle();
        VkResult res = vkCreateImage(vkDevice, &imageCI, nullptr, &image);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateImage failed: %d", res);
            return false;
        }

        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements(vkDevice, image, &memReqs);

        VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (desc.usage & ImageUsageFlagBit::TRANSIENT) {
            props |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        }

        const uint32_t memType = FindMemoryType(memReqs.memoryTypeBits, props);
        if (memType == UINT32_MAX) {
            // fallback without lazily-allocated
            const uint32_t fb = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (fb == UINT32_MAX) {
                LOG_E(TAG, "no suitable memory type for image");
                return false;
            }

            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize  = memReqs.size;
            allocInfo.memoryTypeIndex = fb;
            res = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &memory);
        } else {
            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize  = memReqs.size;
            allocInfo.memoryTypeIndex = memType;
            res = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &memory);
        }

        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkAllocateMemory for image failed: %d", res);
            return false;
        }

        res = vkBindImageMemory(vkDevice, image, memory, 0);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkBindImageMemory failed: %d", res);
            return false;
        }

        owned = true;
        return true;
    }

    void VulkanImage::InitFromSwapChain(VkImage swapImage, VkFormat fmt, const Extent3D &ext)
    {
        image    = swapImage;
        vkFormat = fmt;
        owned    = false;
    }

    uint32_t VulkanImage::FindMemoryType(uint32_t filter, VkMemoryPropertyFlags flags) const
    {
        const auto &memProps = device.GetMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }
        return UINT32_MAX;
    }

} // namespace sky::aurora
