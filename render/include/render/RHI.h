//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/environment/Singleton.h>
#include <rhi/Device.h>
#include <rhi/Instance.h>

namespace sky {

    class RHI : public Singleton<RHI> {
    public:
        RHI() = default;
        ~RHI();

        void InitInstance(const rhi::Instance::Descriptor &desc);
        void InitDevice(const rhi::Device::Descriptor &desc);

        rhi::Device *GetDevice() const { return device.get(); }

    private:
        rhi::Instance *instance = nullptr;
        std::unique_ptr<rhi::Device> device;
    };
}