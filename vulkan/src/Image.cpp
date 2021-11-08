//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Image.h"
#include "vulkan/Device.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    Image::Image(Device& dev) : DevObject(dev), image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE)
    {
    }

    Image::~Image()
    {
        if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device.GetAllocator(), image, allocation);
        }
    }

    bool Image::Init(const Descriptor& des)
    {
        VkImageCreateInfo imageInfo = {};
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

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = des.memory;

        auto rst = vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create image failed, %d", rst);
            return false;
        }
        return true;
    }

    ImageView* Image::CreateImageView(const ImageView::Descriptor& des)
    {
        auto view = new ImageView(device, image);
        if (!view->Init(des)) {
            delete view;
            view = nullptr;
        }
        return view;
    }
}