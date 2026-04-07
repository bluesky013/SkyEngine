//
// Created on 2026/04/02.
//

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanConversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanBuffer::VulkanBuffer(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (mappedPtr != nullptr) {
            vmaUnmapMemory(device.GetAllocator(), allocation);
            mappedPtr = nullptr;
        }
        if (buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(device.GetAllocator(), buffer, allocation);
        }
    }

    bool VulkanBuffer::Init(const Descriptor &desc)
    {
        if (desc.size == 0) {
            LOG_E(TAG, "buffer size must be non-zero");
            return false;
        }

        VkBufferCreateInfo bufferCI = {};
        bufferCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size        = desc.size;
        bufferCI.usage       = FromBufferUsageFlags(desc.usage);
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage = FromMemoryType(desc.memory);

        const VkResult res = vmaCreateBuffer(device.GetAllocator(), &bufferCI, &allocCI, &buffer, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vmaCreateBuffer failed: %d", res);
            return false;
        }

        return true;
    }

    uint8_t *VulkanBuffer::Map()
    {
        if (mappedPtr != nullptr) {
            return mappedPtr;
        }
        const VkResult res = vmaMapMemory(device.GetAllocator(), allocation, reinterpret_cast<void **>(&mappedPtr));
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vmaMapMemory failed: %d", res);
            return nullptr;
        }
        return mappedPtr;
    }

    void VulkanBuffer::UnMap()
    {
        if (mappedPtr != nullptr) {
            vmaUnmapMemory(device.GetAllocator(), allocation);
            mappedPtr = nullptr;
        }
    }

} // namespace sky::aurora
