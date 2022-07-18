//
// Created by Zach Lee on 2021/12/26.
//

#include <render/RenderPipeline.h>
#include <render/DriverManager.h>

namespace sky {

    void RenderPipeline::Setup(RenderViewport& vp)
    {
        auto device = DriverManager::Get()->GetDevice();

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);
        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = commandPool->Allocate(cmdDesc);

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish = device->CreateDeviceObject<drv::Semaphore>({});

        viewport = &vp;
    }
}