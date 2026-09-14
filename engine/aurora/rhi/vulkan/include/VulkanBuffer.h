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

        uint64_t GetId() const { return mId; }

        uint8_t *Map() override;
        void UnMap() override;

    private:
        VulkanDevice  &device;
        VkBuffer       buffer     = VK_NULL_HANDLE;
        VmaAllocation  allocation = VK_NULL_HANDLE;
        uint8_t       *mappedPtr  = nullptr;
        uint64_t       mId        = 0;
    };

} // namespace sky::aurora
