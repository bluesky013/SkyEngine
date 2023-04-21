//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Buffer.h"
#include "core/logger/Logger.h"
#include "vulkan/Conversion.h"
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
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device.GetNativeHandle(), buffer, VKL_ALLOC);
        }
    }

    bool Buffer::Init(const Descriptor &des)
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = des.size;
        bufferInfo.usage = FromRHI(des.usage);

        VkResult res;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage                   = FromRHI(des.memory);
        res                               = vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create buffer failed, %d", res);
            return false;
        }

        bufferDesc = des;
        return true;
    }

    bool Buffer::Init(const VkDescriptor &des)
    {
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = des.size;
        bufferInfo.usage = des.usage;

        VkResult res;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage                   = des.memory;
        res                               = vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create buffer failed, %d", res);
            return false;
        }
        return true;
    }

    uint64_t Buffer::GetSize() const
    {
        return bufferInfo.size;
    }

    VkBuffer Buffer::GetNativeHandle() const
    {
        return buffer;
    }

    bool Buffer::IsTransient() const
    {
        return allocation == VK_NULL_HANDLE;
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

    rhi::BufferViewPtr Buffer::CreateView(const rhi::BufferViewDesc &desc)
    {
        BufferViewPtr ret = std::make_shared<BufferView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::BufferView>(ret);
    }
} // namespace sky::vk
