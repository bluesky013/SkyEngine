//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <memory>
#include <vector>
#include <rhi/Instance.h>
#include <gles/Forward.h>

namespace sky::gles {
    class Device;

    class Instance : public rhi::Instance {
    public:
        Instance() = default;
        ~Instance() override;

        rhi::Device *CreateDevice(const rhi::Device::Descriptor &desc) override;

    private:
        bool Init(const Descriptor &) override;

        bool InitEGL();
        void InitConfig();

        void *eglDisplay{nullptr};
    };
}
