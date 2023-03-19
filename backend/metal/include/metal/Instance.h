//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <string>
#include <vector>
#include <rhi/Instance.h>
#include <rhi/Device.h>
#include <metal/Device.h>

namespace MTL {
    class Device;
}

namespace sky::mtl {
    class Instance : public rhi::Instance {
    public:
        Instance() = default;
        ~Instance() override;

        rhi::Device *CreateDevice(const rhi::Device::Descriptor &desc) override;

        const std::vector<MTL::Device*> &GetMtlDevices() const;

    private:
        bool Init(const Descriptor &) override;
        std::vector<MTL::Device*> devices;
    };

}
