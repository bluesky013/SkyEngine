//
// Created on 2026/04/02.
//

#include <VulkanBuffer.h>

#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanInstance.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    const Device *VulkanBuffer::GetDevice() const noexcept
    {
        return &device;
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice &dev) : device(dev), mId(dev.AllocResourceId())
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
        descriptor = desc;
#if SKY_ENABLE_RESOURCE_NAME
        descriptor.name = nullptr;
#endif
        if (desc.size == 0) {
            LOG_E(TAG, "buffer size must be non-zero");
            return false;
        }

        VkBufferCreateInfo bufferCI = {};
        bufferCI.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size               = desc.size;
        bufferCI.usage              = FromBufferUsageFlags(desc.usage);
        bufferCI.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage                   = FromMemoryType(desc.memory);

        const VkResult res = createResult = vmaCreateBuffer(device.GetAllocator(), &bufferCI, &allocCI, &buffer, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vmaCreateBuffer failed: %d", res);
            return false;
        }

#if SKY_ENABLE_RESOURCE_NAME
        if (desc.name != nullptr) {
            const auto &instanceFn = device.GetVulkanInstance().GetInstanceFn();
            if (instanceFn.vkSetDebugUtilsObjectNameEXT != nullptr) {
                VkDebugUtilsObjectNameInfoEXT nameInfo = {};
                nameInfo.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.objectType                    = VK_OBJECT_TYPE_BUFFER;
                nameInfo.objectHandle                  = reinterpret_cast<uint64_t>(buffer);
                nameInfo.pObjectName                   = desc.name;
                instanceFn.vkSetDebugUtilsObjectNameEXT(device.GetNativeHandle(), &nameInfo);
            }
        }
#endif

        return true;
    }

    bool VulkanBuffer::IsHostCoherent() const noexcept
    {
        VkMemoryPropertyFlags properties = 0;
        vmaGetAllocationMemoryProperties(device.GetAllocator(), allocation, &properties);
        const auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        return (properties & required) == required;
    }

    uint8_t *VulkanBuffer::Map()
    {
        if (mappedPtr != nullptr) {
            return mappedPtr;
        }
        const VkResult res = mapResult = vmaMapMemory(device.GetAllocator(), allocation, reinterpret_cast<void **>(&mappedPtr));
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
