//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/ImageView.h"
#include "core/logger/Logger.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/Image.h"
#include "vulkan/Conversion.h"
static const char *TAG = "Vulkan";

namespace sky::vk {

    ImageView::ImageView(Device &dev) : DevObject(dev), source{}, view{VK_NULL_HANDLE}, viewInfo{}
    {
    }

    ImageView::~ImageView()
    {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device.GetNativeHandle(), view, VKL_ALLOC);
        }
    }

    bool ImageView::Init(const VkDescriptor &des)
    {
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = source->GetNativeHandle();
        viewInfo.viewType         = des.viewType;
        viewInfo.format           = des.format;
        viewInfo.components       = des.components;
        viewInfo.subresourceRange = des.subResourceRange;
        VkResult rst              = vkCreateImageView(device.GetNativeHandle(), &viewInfo, VKL_ALLOC, &view);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create image view failed, -%d", rst);
            return false;
        }
        return true;
    }

    bool ImageView::Init(const rhi::ImageViewDesc &desc)
    {
        viewDesc = desc;

        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = source->GetNativeHandle();
        viewInfo.viewType         = FromRHI(desc.viewType);
        viewInfo.format           = source->GetImageInfo().format;

        viewInfo.components       = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY};

        viewInfo.subresourceRange = {FromRHI(desc.mask),
                                     desc.subRange.baseLevel,
                                     desc.subRange.levels,
                                     desc.subRange.baseLayer,
                                     desc.subRange.layers};

        VkResult rst              = vkCreateImageView(device.GetNativeHandle(), &viewInfo, VKL_ALLOC, &view);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create image view failed, -%d", rst);
        }
        return true;
    }

    std::shared_ptr<rhi::ImageView> ImageView::CreateView(const rhi::ImageViewDesc &desc) const
    {
        return source->CreateView(desc);
    }

    std::shared_ptr<ImageView> ImageView::CreateImageView(const ImagePtr &image, const ImageView::VkDescriptor &des)
    {
        ImageViewPtr ret;
        if (image) {
            ret = std::make_shared<ImageView>(image->device);
            ret->source      = image;
            if (!ret->Init(des)) {
                ret = nullptr;
            }
        }
        return ret;
    }

} // namespace sky::vk
