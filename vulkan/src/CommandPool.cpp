//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/CommandPool.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    CommandPool::CommandPool(Device& dev) : DevObject(dev), pool(VK_NULL_HANDLE)
    {
    }

    CommandPool::~CommandPool()
    {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool CommandPool::Init(const Descriptor& des)
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = des.queueFamilyIndex;
        poolInfo.flags = des.flag;
        VkResult rst = vkCreateCommandPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create command pool failed -%u", rst);
            return false;
        }
        return true;
    }

    CommandBufferPtr CommandPool::Allocate(const CommandBuffer::Descriptor& des)
    {
        VkCommandBufferAllocateInfo cbInfo = {};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbInfo.commandPool = pool;
        cbInfo.level = des.level;
        cbInfo.commandBufferCount = 1;

        VkCommandBuffer buffer = VK_NULL_HANDLE;
        VkResult rst = vkAllocateCommandBuffers(device.GetNativeHandle(), &cbInfo, &buffer);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "allocate command buffer failed， %d", rst);
            return nullptr;
        }

        auto cmdBuffer = new CommandBuffer(device, pool, buffer);
        if (!cmdBuffer->Init(des)) {
            delete cmdBuffer;
            cmdBuffer = nullptr;
        }
        return std::shared_ptr<CommandBuffer>(cmdBuffer);
    }
}