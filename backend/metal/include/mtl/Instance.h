//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <string>
#include <vector>
#include <rhi/Instance.h>
#include <rhi/Device.h>
#include <mtl/Device.h>

namespace sky::mtl {
    class Instance : public rhi::Instance {
    public:
        Instance() = default;
        ~Instance() noexcept override;

        rhi::Device *CreateDevice(const rhi::Device::Descriptor &desc) override;

        const std::vector<id<MTLDevice>> &GetMtlDevices() const { return devices; }

    private:
        bool Init(const Descriptor &) override;

        std::vector<id<MTLDevice>> devices;
    };

}
