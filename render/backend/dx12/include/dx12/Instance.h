//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <string>
#include <vector>
#include <RHI/Instance.h>

#include <dx12/Base.h>
#include <dx12/Device.h>

namespace sky::dx {

    class Instance : public rhi::Instance {
    public:
        Instance() = default;
        ~Instance() override = default;

        Device *CreateDevice(const Device::Descriptor &);
        IDXGIFactory2 *GetDXGIFactory() const;

    private:
        bool Init(const Descriptor &) override;
        ComPtr<IDXGIFactory2> dxgiFactory;
        std::vector<ComPtr<IDXGIAdapter1>> adapters;
    };

}