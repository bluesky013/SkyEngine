//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <RHI/Device.h>

namespace sky::gles {

    class Device : public rhi::Device {
    public:
        Device() = default;
        ~Device() = default;

        bool Init(const Descriptor &desc);
    };

}
