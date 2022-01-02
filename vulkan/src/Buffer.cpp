//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Buffer.h"
#include "vulkan/Device.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    Buffer::Buffer(Device& dev) : DevObject(dev), buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE)
    {
    }

    Buffer::~Buffer()
    {
        if (buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(device.GetAllocator(), buffer, allocation);
        }
    }

    bool Buffer::Init(const Descriptor& des)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = des.size;
        bufferInfo.usage = des.usage;

        VkResult res;
        if (des.allocate) {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = des.memory;
            res = vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
        } else {
            res = vkCreateBuffer(device.GetNativeHandle(), &bufferInfo, nullptr, &buffer);
        }
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create buffer failed, %d", res);
            return false;
        }
        return true;
    }

    VkBuffer Buffer::GetNativeHandle() const
    {
        return buffer;
    }
}