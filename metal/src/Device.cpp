//
// Created by Zach Lee on 2022/11/4.
//

#include <metal/Device.h>
#include <metal/Instance.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    bool Device::Init(const Descriptor &des)
    {
        const auto &mtlDevices = instance.GetMtlDevices();
        device = mtlDevices.front();
    }

    MTL::Device *Device::GetMetalDevice() const
    {
        return device;
    }
}
