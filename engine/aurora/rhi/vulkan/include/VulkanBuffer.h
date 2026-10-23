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

        bool     Init(const Descriptor &desc);
        VkResult GetCreateResult() const noexcept
        {
            return createResult;
        }

        VkBuffer GetNativeHandle() const
        {
            return buffer;
        }

        uint64_t GetId() const
        {
            return mId;
        }
        uint64_t GetSize() const override
        {
            return descriptor.size;
        }
        const Descriptor &GetDescriptor() const noexcept override
        {
            return descriptor;
        }
        const Device *GetDevice() const noexcept override;
        bool          IsHostCoherent() const noexcept;

        VkResult GetMapResult() const noexcept
        {
            return mapResult;
        }
        uint8_t *Map() override;
        void     UnMap() override;

    private:
        VkResult      createResult = VK_SUCCESS;
        VkResult      mapResult    = VK_SUCCESS;
        VulkanDevice &device;
        Descriptor    descriptor;
        VkBuffer      buffer     = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        uint8_t      *mappedPtr  = nullptr;
        uint64_t      mId        = 0;
    };

} // namespace sky::aurora
