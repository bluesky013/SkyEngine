//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Device.h>
#include <mtl/Instance.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    bool Device::Init(const Descriptor &des)
    {
        const auto &mtlDevices = instance.GetMtlDevices();
        device = mtlDevices.front();
        return true;
    }

    MTL::Device *Device::GetMetalDevice() const
    {
        return device;
    }
}
