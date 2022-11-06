//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Device.h>
#include <dx12/Instance.h>

namespace sky::dx {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    bool Device::Init(const Descriptor &desc, ComPtr<IDXGIAdapter1> &adaptor)
    {
        if (FAILED(D3D12CreateDevice(adaptor.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())))) {
            return false;
        }

        graphicsQueue = std::make_unique<Queue>(*this);
        graphicsQueue->Init({});

        return true;
    }

    ID3D12Device *Device::GetDevice() const
    {
        return device.Get();
    }

    IDXGIFactory2 *Device::GetDXGIFactory() const
    {
        return instance.GetDXGIFactory();
    }

    Queue *Device::GetGraphicsQueue() const
    {
        return graphicsQueue.get();
    }
}