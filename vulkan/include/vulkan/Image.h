//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <vulkan/ImageView.h>
#include <vk_mem_alloc.h>
#include <list>
#include <unordered_map>
#include <mutex>

namespace sky::drv {

    class Device;

    class Image : public DevObject {
    public:
        ~Image();

        struct Descriptor {
            VkImageType           imageType   = VK_IMAGE_TYPE_2D;
            VkFormat              format      = VK_FORMAT_UNDEFINED;
            VkExtent3D            extent      = {1, 1, 1};
            uint32_t              mipLevels   = 1;
            uint32_t              arrayLayers = 1;
            VkImageUsageFlags     usage       = 0;
            VkSampleCountFlagBits samples     = VK_SAMPLE_COUNT_1_BIT;
            VkImageTiling         tiling      = VK_IMAGE_TILING_OPTIMAL;
            VmaMemoryUsage        memory      = VMA_MEMORY_USAGE_UNKNOWN;
            bool                  transient   = false;
            char                  rsv[3]      = {0};
        };

        ImageViewPtr CreateImageView(const ImageView::Descriptor& des);

        bool IsTransient() const;

        const VkImageCreateInfo& GetImageInfo() const;

        VkImage GetNativeHandle() const;

    private:
        friend class Device;
        friend class ImageView;
        friend class SwapChain;
        Image(Device&);

        Image(Device&, VkImage);

        bool Init(const Descriptor&);

        void Reset();

        VkImage image;
        VmaAllocation allocation;
        VkImageCreateInfo imageInfo;
        bool isTransient = false;
        bool isOwn = true;
    };

    using ImagePtr = std::shared_ptr<Image>;

}