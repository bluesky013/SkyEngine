//
// Created by Zach Lee on 2022/6/21.
//

#include <render/RenderViewport.h>
#include <render/DriverManager.h>

namespace sky {

    RenderViewport::~RenderViewport()
    {
        Shutdown();
    }

    void RenderViewport::SetScene(RDScenePtr scn)
    {
        scene = scn;
        scene->Setup(*this);
    }

    void RenderViewport::Setup(const ViewportInfo& info)
    {
        drv::SwapChain::Descriptor descriptor = {};
        descriptor.window = info.wHandle;

        swapChain = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::SwapChain>(descriptor);
        nativeHandle = info.wHandle;
        Event<IWindowEvent>::Connect(descriptor.window, this);
    }

    void RenderViewport::Shutdown()
    {
        if (swapChain != nullptr) {
            auto device = DriverManager::Get()->GetDevice();
            device->WaitIdle();
            swapChain = nullptr;
        }
        Event<IWindowEvent>::DisConnect(this);
    }

    drv::SwapChainPtr RenderViewport::GetSwapChain() const
    {
        return swapChain;
    }

    void* RenderViewport::GetNativeHandle() const
    {
        return nativeHandle;
    }

    void RenderViewport::OnWindowResize(uint32_t width, uint32_t height)
    {
        swapChain->Resize(width, height);
        scene->ViewportChange(*this);
    }

}