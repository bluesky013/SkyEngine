//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vk_mem_alloc.h>
#include <rhi/ImageView.h>

#include <vulkan/DevObject.h>
#include <vulkan/Image.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class Device;
    class Image;

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &);
        ~ImageView();

        struct VkDescriptor {
            VkImageViewType         viewType         = VK_IMAGE_VIEW_TYPE_2D;
            VkFormat                format           = VK_FORMAT_UNDEFINED;
            VkComponentMapping      components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY};
            VkImageSubresourceRange subResourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        };

        static std::shared_ptr<ImageView> CreateImageView(const ImagePtr &image, const ImageView::VkDescriptor &des);
        std::shared_ptr<rhi::ImageView> CreateView(const rhi::ImageViewDesc &desc) const override;
        rhi::PixelFormat ImageView::GetFormat() const override;
        const rhi::Extent3D &ImageView::GetExtent() const override;

        VkImageView GetNativeHandle() const { return view; }
        const ImagePtr &GetImage()  const { return source; }
        const VkImageSubresourceRange &GetSubRange() const { return viewInfo.subresourceRange; }

    private:
        friend class Image;
        friend class SwapChain;

        bool Init(const rhi::ImageViewDesc &);
        bool Init(const VkDescriptor &);

        ImagePtr              source;
        VkImageView           view;
        VkImageViewCreateInfo viewInfo;
    };

    using ImageViewPtr = std::shared_ptr<ImageView>;
} // namespace sky::vk
