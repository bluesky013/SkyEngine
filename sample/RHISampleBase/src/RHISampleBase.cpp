//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"

namespace sky::rhi {

    void RHISampleBase::OnStart()
    {
        instance = Instance::Create({"", "", false, API::GLES});
        device   = instance->CreateDevice({});

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
        SwapChain::Descriptor swcDesc = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swcDesc.width  = nativeWindow->GetWidth();
        swcDesc.height = nativeWindow->GetHeight();
        swapChain      = device->CreateSwapChain(swcDesc);
    }

    void RHISampleBase::OnStop()
    {
        delete device;
        device = nullptr;

        Instance::Destroy(instance);
        instance = nullptr;
    }

    void RHISampleBase::OnTick(float delta)
    {

    }

    void RHISampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }
}
