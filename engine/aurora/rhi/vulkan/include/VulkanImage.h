//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Image.h>
#include <vk_mem_alloc.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanImage : public Image {
    public:
        explicit VulkanImage(VulkanDevice &dev);
        ~VulkanImage() override;

        bool Init(const Descriptor &desc);

        // Adopt an externally-owned image (e.g. swapchain image). Not destroyed on cleanup.
        void InitFromSwapChain(VkImage swapImage, VkFormat fmt, PixelFormat pixFmt, const Extent3D &ext);

        VkImage GetNativeHandle() const
        {
            return image;
        }
        VkImageView GetDefaultView() const
        {
            return defaultView;
        }
        VkFormat GetVkFormat() const
        {
            return vkFormat;
        }
        PixelFormat GetPixelFormat() const
        {
            return pixelFormat;
        }

        uint64_t GetId() const
        {
            return mId;
        }
        uint64_t GetMipLayerCount() const noexcept override
        {
            return uint64_t(descriptor.mipLevels) * descriptor.arrayLayers;
        }
        const Descriptor &GetDescriptor() const noexcept
        {
            return descriptor;
        }
        const Device *GetDevice() const noexcept override;
        bool          IsOwned() const noexcept
        {
            return owned;
        }

    private:
        bool CreateDefaultView(const Descriptor &desc);
        bool CreateSwapChainDefaultView();

        VulkanDevice &device;
        Descriptor    descriptor;
        VkImage       image       = VK_NULL_HANDLE;
        VkImageView   defaultView = VK_NULL_HANDLE;
        VmaAllocation allocation  = VK_NULL_HANDLE;
        VkFormat      vkFormat    = VK_FORMAT_UNDEFINED;
        PixelFormat   pixelFormat = PixelFormat::UNDEFINED;
        bool          owned       = true;
        uint64_t      mId         = 0;
    };

} // namespace sky::aurora
