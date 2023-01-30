//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <RHI/Instance.h>

namespace sky::gles {
    class Device;

    class Instance : public rhi::Instance {
    public:
        Instance() = default;
        ~Instance() = default;

        rhi::Device *CreateDevice(const rhi::Device::Descriptor &desc) override;
    };
}
