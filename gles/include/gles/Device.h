//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <rhi/Device.h>
#include <gles/Context.h>
#include <memory>

namespace sky::gles {

    class Device : public rhi::Device {
    public:
        Device() = default;
        ~Device() = default;

        bool Init(const Descriptor &desc);

    private:
        std::unique_ptr<Context> mainContext;
        std::unique_ptr<Context> graphicsContext;
        std::unique_ptr<Context> transferContext;
    };

}
