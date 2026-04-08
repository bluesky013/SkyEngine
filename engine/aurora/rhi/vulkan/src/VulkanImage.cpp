//
// Created on 2026/04/02.
//

#include "VulkanImage.h"
#include "VulkanDevice.h"
#include "VulkanConversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    static VkImageAspectFlags InferImageAspectFlags(VkFormat format)
    {
        switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    static VkImageViewType InferDefaultViewType(const Image::Descriptor &desc)
    {
        switch (desc.imageType) {
        case ImageType::IMAGE_1D:
            return desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case ImageType::IMAGE_2D:
            if (desc.viewUsage & ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE) {
                return desc.arrayLayers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
            }
            return desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case ImageType::IMAGE_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        default:
            return VK_IMAGE_VIEW_TYPE_2D;
        }
    }

    VulkanImage::VulkanImage(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanImage::~VulkanImage()
    {
        if (defaultView != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyImageView(device.GetNativeHandle(), defaultView, nullptr);
        }
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
        return CreateDefaultView(desc);
    }

    void VulkanImage::InitFromSwapChain(VkImage swapImage, VkFormat fmt, const Extent3D &ext)
    {
        image    = swapImage;
        vkFormat = fmt;
        owned    = false;
        if (!CreateSwapChainDefaultView()) {
            LOG_E(TAG, "failed to create default image view for swapchain image");
        }
    }

    bool VulkanImage::CreateDefaultView(const Descriptor &desc)
    {
        VkImageViewCreateInfo viewCI = {};
        viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image                           = image;
        viewCI.viewType                        = InferDefaultViewType(desc);
        viewCI.format                          = vkFormat;
        viewCI.subresourceRange.aspectMask     = InferImageAspectFlags(vkFormat);
        viewCI.subresourceRange.baseMipLevel   = 0;
        viewCI.subresourceRange.levelCount     = desc.mipLevels;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.layerCount     = desc.arrayLayers;

        const VkResult result = device.GetDeviceFn().vkCreateImageView(device.GetNativeHandle(), &viewCI, nullptr, &defaultView);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create default VkImageView, error: %d", result);
            return false;
        }
        return true;
    }

    bool VulkanImage::CreateSwapChainDefaultView()
    {
        VkImageViewCreateInfo viewCI = {};
        viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image                           = image;
        viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format                          = vkFormat;
        viewCI.subresourceRange.aspectMask     = InferImageAspectFlags(vkFormat);
        viewCI.subresourceRange.baseMipLevel   = 0;
        viewCI.subresourceRange.levelCount     = 1;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.layerCount     = 1;

        const VkResult result = device.GetDeviceFn().vkCreateImageView(device.GetNativeHandle(), &viewCI, nullptr, &defaultView);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create swapchain VkImageView, error: %d", result);
            return false;
        }
        return true;
    }

} // namespace sky::aurora
