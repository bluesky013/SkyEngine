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
        explicit ImageView(Device &);
        ~ImageView() override;

        std::shared_ptr<rhi::ImageView> CreateView(const rhi::ImageViewDesc &desc) const override;
        rhi::PixelFormat GetFormat() const override;
        const rhi::Extent3D &GetExtent() const override;

        VkImageView GetNativeHandle() const { return view; }
        const ImagePtr &GetImage()  const { return source; }
        const VkImageSubresourceRange &GetSubRange() const { return viewInfo.subresourceRange; }

    private:
        friend class Image;
        friend class SwapChain;

        bool Init(const rhi::ImageViewDesc &);

        ImagePtr              source;
        VkImageView           view;
        VkImageViewCreateInfo viewInfo;
    };

    using ImageViewPtr = std::shared_ptr<ImageView>;
} // namespace sky::vk
