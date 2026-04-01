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
        const auto vkDevice = device.GetNativeHandle();
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(vkDevice, buffer, nullptr);
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(vkDevice, memory, nullptr);
        }
    }

    bool VulkanBuffer::Init(const Descriptor &desc)
    {
        size = desc.size;
        if (size == 0) {
            LOG_E(TAG, "buffer size must be non-zero");
            return false;
        }

        VkBufferCreateInfo bufferCI = {};
        bufferCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size        = size;
        bufferCI.usage       = FromBufferUsageFlags(desc.usage);
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const auto vkDevice = device.GetNativeHandle();
        VkResult res = vkCreateBuffer(vkDevice, &bufferCI, nullptr, &buffer);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateBuffer failed: %d", res);
            return false;
        }

        VkMemoryRequirements memReqs = {};
        vkGetBufferMemoryRequirements(vkDevice, buffer, &memReqs);

        VkMemoryPropertyFlags props = 0;
        switch (desc.memory) {
        case MemoryType::GPU_ONLY:
            props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case MemoryType::CPU_ONLY:
        case MemoryType::CPU_TO_GPU:
            props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case MemoryType::GPU_TO_CPU:
            props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            break;
        }
        memFlags = props;

        const uint32_t memType = FindMemoryType(memReqs.memoryTypeBits, props);
        if (memType == UINT32_MAX) {
            LOG_E(TAG, "no suitable memory type for buffer");
            return false;
        }

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = memType;

        res = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &memory);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkAllocateMemory for buffer failed: %d", res);
            return false;
        }

        res = vkBindBufferMemory(vkDevice, buffer, memory, 0);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkBindBufferMemory failed: %d", res);
            return false;
        }

        return true;
    }

    uint8_t *VulkanBuffer::Map()
    {
        if (memory == VK_NULL_HANDLE) {
            return nullptr;
        }
        void *data = nullptr;
        const VkResult res = vkMapMemory(device.GetNativeHandle(), memory, 0, size, 0, &data);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkMapMemory failed: %d", res);
            return nullptr;
        }
        return static_cast<uint8_t *>(data);
    }

    void VulkanBuffer::UnMap()
    {
        if (memory != VK_NULL_HANDLE) {
            vkUnmapMemory(device.GetNativeHandle(), memory);
        }
    }

    uint32_t VulkanBuffer::FindMemoryType(uint32_t filter, VkMemoryPropertyFlags flags) const
    {
        const auto &memProps = device.GetMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }
        return UINT32_MAX;
    }

} // namespace sky::aurora
