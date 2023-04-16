//
// Created by Zach Lee on 2023/4/16.
//

#include <render/RHI.h>

namespace sky {

    RHI::~RHI()
    {
        device = nullptr;
        rhi::Instance::Destroy(instance);
        instance = nullptr;
    }

    void RHI::InitInstance(const rhi::Instance::Descriptor &desc)
    {
        instance = rhi::Instance::Create(desc);
    }

    void RHI::InitDevice(const rhi::Device::Descriptor &desc)
    {
        device.reset(instance->CreateDevice(desc));
    }
}