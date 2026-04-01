//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Image.h>
#include <vulkan/vulkan.h>

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
        VkFormat GetVkFormat() const { return vkFormat; }

    private:
        uint32_t FindMemoryType(uint32_t filter, VkMemoryPropertyFlags flags) const;

        VulkanDevice   &device;
        VkImage         image    = VK_NULL_HANDLE;
        VkDeviceMemory  memory   = VK_NULL_HANDLE;
        VkFormat        vkFormat = VK_FORMAT_UNDEFINED;
        bool            owned    = true;
    };

} // namespace sky::aurora
