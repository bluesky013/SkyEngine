//
// Created by Zach Lee on 2022/11/5.
//

#include "DXSampleBase.h"

namespace sky {

    void DX12SampleBase::OnStart()
    {
        instance = dx::Instance::Create({});
        device = instance->CreateDevice({});
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