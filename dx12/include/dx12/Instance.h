//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <string>
#include <vector>
#include <dx12/Basic.h>
#include <dx12/Device.h>

namespace sky::dx {

    class Instance {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
        };

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &);

    private:
        Instance() = default;
        ~Instance() = default;

        bool Init(const Descriptor &);

        CompPtr<IDXGIFactory6> dxgiFactory;
        std::vector<CompPtr<IDXGIAdapter1>> adapters;
    };

}