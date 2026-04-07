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
        if (owned && image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device.GetAllocator(), image, allocation);
        } else if (owned && image != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyImage(device.GetNativeHandle(), image, nullptr);
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

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage = FromMemoryType(desc.memory, desc.usage);

        VkResult res = vmaCreateImage(device.GetAllocator(), &imageCI, &allocCI, &image, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            // fallback: GPU_LAZILY_ALLOCATED may not be supported
            if (allocCI.usage == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED) {
                LOG_W(TAG, "memoryless not supported, falling back to GPU_ONLY");
                allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
                imageCI.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
                res = vmaCreateImage(device.GetAllocator(), &imageCI, &allocCI, &image, &allocation, nullptr);
            }
            if (res != VK_SUCCESS) {
                LOG_E(TAG, "vmaCreateImage failed: %d", res);
                return false;
            }
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

} // namespace sky::aurora
