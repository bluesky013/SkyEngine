//
// Created by Zach Lee on 2021/12/26.
//

#include <render/RenderPipeline.h>
#include <render/DriverManager.h>
#include <render/RenderScene.h>

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

    void RenderPipeline::DoFrame()
    {
        auto& views = scene.GetViews();
        for (auto& view : views) {
            auto& primitives = view->GetPrimitives();
            for (auto& primitive : primitives) {
                auto& techniques = primitive->GetTechniques();
                for (auto& tech : techniques) {
                    for (auto& encoder : encoders) {
                        if ((encoder->GetDrawTag() & tech->drawTag) == 0) {
                            continue;
                        }
                        drv::DrawItem item;
                        item.pso = tech->pso;
                        item.vertexAssembly = tech->assembly;
                        item.drawArgs = tech->args;
                        item.shaderResources = tech->setBinder;
                        encoder->Emplace(item);
                    }
                }
            }
        }
    }
}