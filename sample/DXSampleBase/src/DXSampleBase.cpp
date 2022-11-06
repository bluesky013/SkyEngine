//
// Created by Zach Lee on 2022/11/5.
//

#include "DXSampleBase.h"

namespace sky {

    void DX12SampleBase::OnStart()
    {
        instance = dx::Instance::Create({});
        device   = instance->CreateDevice({});

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        dx::SwapChain::Descriptor swcDesc = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swcDesc.width  = nativeWindow->GetWidth();
        swcDesc.height = nativeWindow->GetHeight();
        swapChain      = device->CreateDeviceObject<dx::SwapChain>(swcDesc);

        Event<IWindowEvent>::Connect(swcDesc.window, this);
    }

    void DX12SampleBase::OnStop()
    {
        delete device;
        device = nullptr;

        dx::Instance::Destroy(instance);
        instance = nullptr;
    }

    void DX12SampleBase::OnTick(float delta)
    {

    }

    void DX12SampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }
}