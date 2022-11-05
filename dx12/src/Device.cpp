//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Device.h>

namespace sky::dx {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    bool Device::Init(const Descriptor &desc, CompPtr<IDXGIAdapter1> adaptor)
    {
        if (!FAILED(D3D12CreateDevice(adaptor.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())))) {
            return false;
        }
        return true;
    }
}