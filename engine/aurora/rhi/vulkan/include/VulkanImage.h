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
        void InitFromSwapChain(VkImage swapImage, VkFormat fmt, const Extent3D &ext);

        VkImage  GetNativeHandle() const { return image; }
        VkImageView GetDefaultView() const { return defaultView; }
        VkFormat GetVkFormat() const { return vkFormat; }

    private:
        bool CreateDefaultView(const Descriptor &desc);
        bool CreateSwapChainDefaultView();

        VulkanDevice  &device;
        VkImage        image      = VK_NULL_HANDLE;
        VkImageView    defaultView = VK_NULL_HANDLE;
        VmaAllocation  allocation = VK_NULL_HANDLE;
        VkFormat       vkFormat   = VK_FORMAT_UNDEFINED;
        bool           owned      = true;
    };

} // namespace sky::aurora
