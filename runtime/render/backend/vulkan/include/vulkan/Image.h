//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <list>
#include <mutex>
#include <unordered_map>
#include <vk_mem_alloc.h>

#include <rhi/Image.h>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class Device;
    class ImageView;

    class Image : public rhi::Image, public DevObject, public std::enable_shared_from_this<Image> {
    public:
        ~Image() override;

        struct VkDescriptor {
            VkImageType           imageType   = VK_IMAGE_TYPE_2D;
            VkFormat              format      = VK_FORMAT_UNDEFINED;
            VkExtent3D            extent      = {1, 1, 1};
            uint32_t              mipLevels   = 1;
            uint32_t              arrayLayers = 1;
            VkImageUsageFlags     usage       = 0;
            VkSampleCountFlagBits samples     = VK_SAMPLE_COUNT_1_BIT;
            VkImageTiling         tiling      = VK_IMAGE_TILING_OPTIMAL;
            VmaMemoryUsage        memory      = VMA_MEMORY_USAGE_UNKNOWN;
            VkImageCreateFlags    flags       = 0;
            bool                  allocateMem = true;
        };

        bool IsTransient() const;

        const VkImageCreateInfo &GetImageInfo() const;

        VkImage GetNativeHandle() const;

        void BindMemory(VmaAllocation allocation);
        void ReleaseMemory();

    private:
        friend class Device;
        friend class ImageView;
        friend class SwapChain;
        friend class XRSwapChain;
        rhi::ImageViewPtr CreateView(const rhi::ImageViewDesc &desc) override;

        explicit Image(Device &);
        explicit Image(Device &, VkImage);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        void Reset();

        VkImage           image;
        VmaAllocation     allocation;
        VkImageCreateInfo imageInfo;
        bool              isOwn       = true;
    };

    using ImagePtr = std::shared_ptr<Image>;

} // namespace sky::vk
