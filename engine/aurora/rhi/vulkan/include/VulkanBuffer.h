//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Buffer.h>
#include <vk_mem_alloc.h>

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
        VulkanDevice  &device;
        VkBuffer       buffer     = VK_NULL_HANDLE;
        VmaAllocation  allocation = VK_NULL_HANDLE;
        uint8_t       *mappedPtr  = nullptr;
    };

} // namespace sky::aurora
