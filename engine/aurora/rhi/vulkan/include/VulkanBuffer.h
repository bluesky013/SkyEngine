//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Buffer.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanBuffer : public Buffer {
    public:
        explicit VulkanBuffer(VulkanDevice &dev);
        ~VulkanBuffer() override;

        bool Init(const Descriptor &desc);

        VkBuffer GetNativeHandle() const { return buffer; }

        uint8_t *Map();
        void UnMap();

    private:
        uint32_t FindMemoryType(uint32_t filter, VkMemoryPropertyFlags flags) const;

        VulkanDevice      &device;
        VkBuffer           buffer     = VK_NULL_HANDLE;
        VkDeviceMemory     memory     = VK_NULL_HANDLE;
        uint64_t           size       = 0;
        VkMemoryPropertyFlags memFlags = 0;
    };

} // namespace sky::aurora
