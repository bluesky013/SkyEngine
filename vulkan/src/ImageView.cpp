//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/ImageView.h"
#include "vulkan/Image.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "core/logger/Logger.h"
static const char* TAG = "Driver";

namespace sky::drv {

    ImageView::ImageView(Device& dev)
        : DevObject(dev)
        , source{}
        , view{VK_NULL_HANDLE}
        , viewInfo{}
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
        viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image      = source->GetNativeHandle();
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

    const VkImageViewCreateInfo& ImageView::GetViewInfo() const
    {
        return viewInfo;
    }

    ImageView::Descriptor ImageView::Make2DColor(VkFormat format)
    {
        ImageView::Descriptor desc = {};
        desc.format = format;
        desc.subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        return desc;
    }

    ImageView::Descriptor ImageView::Make2DDepth(VkFormat format)
    {
        ImageView::Descriptor desc = {};
        desc.format = format;
        desc.subResourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        return desc;
    }

    ImageView::Descriptor ImageView::Make2DDepthStencil(VkFormat format)
    {
        ImageView::Descriptor desc = {};
        desc.format = format;
        desc.subResourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        return desc;
    }

    std::shared_ptr<ImageView> ImageView::CreateImageView(ImagePtr image, ImageView::Descriptor& des)
    {
        ImageViewPtr ptr = std::make_shared<ImageView>(image->device);
        ptr->source = image;
        if (ptr->Init(des)) {
            return ptr;
        }
        return {};
    }
}