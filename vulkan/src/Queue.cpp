//
// Created by Zach Lee on 2022/1/3.
//

#include <vulkan/Queue.h>
#include <vulkan/Device.h>

namespace sky::drv {

    void Queue::Setup()
    {
        CommandPool::Descriptor des = {};
        des.queueFamilyIndex = queueFamilyIndex;
        des.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        pool = device.CreateDeviceObject<CommandPool>(des);
    }

    CommandBufferPtr Queue::AllocateCommandBuffer(const CommandBuffer::Descriptor& des)
    {
        return pool->Allocate(des);
    }

}