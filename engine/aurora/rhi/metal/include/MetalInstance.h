//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Instance.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalInstance : public Instance::Impl {
    public:
        MetalInstance() = default;
        ~MetalInstance() override;

        bool Init(const Instance::Descriptor &desc) override;
        Device *CreateDevice() override;

        void *GetNativeDevice() const { return metalDevice; }

    private:
        void *metalDevice = nullptr;
    };

} // namespace sky::aurora