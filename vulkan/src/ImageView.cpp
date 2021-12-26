//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/ImageView.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "core/logger/Logger.h"
static const char* TAG = "Driver";

namespace sky::drv {

    ImageView::ImageView(Device& dev) : DevObject(dev), image(VK_NULL_HANDLE), view(VK_NULL_HANDLE)
    {
    }

    ImageView::~ImageView()
    {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device.GetNativeHandle(), view, VKL_ALLOC);
        }
    }

    bool ImageView::Init(const Descriptor& des)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image      = image;
        viewInfo.viewType   = des.viewType;
        viewInfo.format     = des.format;
        viewInfo.components = des.components;
        viewInfo.subresourceRange = des.subResourceRange;
        VkResult rst = vkCreateImageView(device.GetNativeHandle(), &viewInfo, VKL_ALLOC, &view);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create image view failed, -%d", rst);
        }
        return true;
    }

    VkImageView ImageView::GetNativeHandle() const
    {
        return view;
    }
}