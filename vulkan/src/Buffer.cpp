//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Buffer.h"
#include "core/logger/Logger.h"
#include "vulkan/Device.h"
#include "vk_mem_alloc.h"

static const char *TAG = "Vulkan";

namespace sky::vk {

    Buffer::Buffer(Device &dev) : DevObject(dev), buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), bufferInfo{}
    {
    }

    Buffer::~Buffer()
    {
        if (buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            if (mappedPtr != nullptr) {
                vmaUnmapMemory(device.GetAllocator(), allocation);
            }
            vmaDestroyBuffer(device.GetAllocator(), buffer, allocation);
        }
    }

    bool Buffer::Init(const Descriptor &des)
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = des.size;
        bufferInfo.usage = des.usage;

        VkResult res;
        if (!des.transient) {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage                   = des.memory;
            res                               = vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
        } else {
            res = vkCreateBuffer(device.GetNativeHandle(), &bufferInfo, nullptr, &buffer);
        }
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create buffer failed, %d", res);
            return false;
        }

        isTransient = des.transient;
        desc        = des;
        return true;
    }

    VkBuffer Buffer::GetNativeHandle() const
    {
        return buffer;
    }

    bool Buffer::IsTransient() const
    {
        return isTransient;
    }

    uint8_t *Buffer::Map()
    {
        if (mappedPtr == nullptr) {
            vmaMapMemory(device.GetAllocator(), allocation, reinterpret_cast<void **>(&mappedPtr));
        }
        return mappedPtr;
    }

    void Buffer::UnMap()
    {
    }

    void Buffer::BindMemory(VmaAllocation alloc)
    {
        allocation = alloc;
        vmaBindBufferMemory(device.GetAllocator(), allocation, buffer);
    }

    void Buffer::ReleaseMemory()
    {
        vmaFreeMemory(device.GetAllocator(), allocation);
        allocation = VK_NULL_HANDLE;
    }
} // namespace sky::vk
