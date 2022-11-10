//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <string>
#include <vector>
#include <RHI/Instance.h>

#include <dx12/Basic.h>
#include <dx12/Device.h>

namespace sky::dx {

    class Instance : public rhi::Instance {
    public:
        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &);

        IDXGIFactory2 *GetDXGIFactory() const;

    private:
        Instance() = default;
        ~Instance() override = default;

        bool Init(const Descriptor &);

        ComPtr<IDXGIFactory2> dxgiFactory;
        std::vector<ComPtr<IDXGIAdapter1>> adapters;
    };

}