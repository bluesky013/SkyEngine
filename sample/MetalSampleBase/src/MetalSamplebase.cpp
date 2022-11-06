//
// Created by Zach Lee on 2022/11/4.
//

#include "MetalSampleBase.h"

namespace sky {

    void MetalSampleBase::Init()
    {
        mtl::Instance::Descriptor drvDes = {};
        drvDes.enableDebugLayer          = true;
        drvDes.appName                   = "Triangle";
        instance                         = mtl::Instance::Create(drvDes);
        if (instance == nullptr) {
            return;
        }
        device = instance->CreateDevice({});
    }

    void MetalSampleBase::Start()
    {

        OnStart();
    }

    void MetalSampleBase::Stop()
    {
        OnStop();
    }

    void MetalSampleBase::Tick(float delta)
    {

    }

    void MetalSampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }
}

REGISTER_MODULE(sky::MetalSampleBase)
