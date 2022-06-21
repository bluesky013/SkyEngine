//
// Created by Zach Lee on 2022/6/21.
//

#include <render/RenderViewport.h>
#include <render/DriverManager.h>

namespace sky {

    RenderViewport::~RenderViewport()
    {
    }

    void RenderViewport::SetScene(RDScenePtr scene)
    {
        scene = scene;
    }

    void RenderViewport::Setup(const ViewportInfo& info)
    {
        drv::SwapChain::Descriptor descriptor = {};
        descriptor.window = info.wHandle;

        swapChain = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::SwapChain>(descriptor);
    }

    void RenderViewport::Shutdown()
    {
        auto device = DriverManager::Get()->GetDevice();
        device->WaitIdle();

        swapChain = nullptr;
    }

}