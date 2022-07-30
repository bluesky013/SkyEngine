//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Image.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "core/logger/Logger.h"
#include "core/hash/Crc32.h"

static const char* TAG = "Driver";

namespace sky::drv {

    Image::Image(Device& dev)
        : DevObject(dev)
        , image(VK_NULL_HANDLE)
        , allocation(VK_NULL_HANDLE)
        , imageInfo{}
    {
    }

    Image::Image(Device& dev, VkImage img)
        : DevObject(dev)
        , image(img)
        , allocation(VK_NULL_HANDLE)
        , imageInfo{}
        , isOwn(false)
    {
    }

    Image::~Image()
    {
        Reset();
    }

    bool Image::Init(const Descriptor& des)
    {
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.mipLevels     = des.mipLevels;
        imageInfo.arrayLayers   = des.arrayLayers;
        imageInfo.format        = des.format;
        imageInfo.extent        = des.extent;
        imageInfo.imageType     = des.imageType;
        imageInfo.usage         = des.usage;
        imageInfo.samples       = des.samples;
        imageInfo.tiling        = des.tiling;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult res;
        if (!des.transient) {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = des.memory;

            res = vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
        } else {
            res = vkCreateImage(device.GetNativeHandle(), &imageInfo, nullptr, &image);
        }
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create image failed %d", res);
            return false;
        }

        isTransient = des.transient;
        return true;
    }

    void Image::Reset()
    {
        if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device.GetAllocator(), image, allocation);
        } else if (image != VK_NULL_HANDLE && isOwn) {
            vkDestroyImage(device.GetNativeHandle(), image, VKL_ALLOC);
        }
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    bool Image::IsTransient() const
    {
        return isTransient;
    }

    const VkImageCreateInfo& Image::GetImageInfo() const
    {
        return imageInfo;
    }

    VkImage Image::GetNativeHandle() const
    {
        return image;
    }
}