//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Sampler.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalSampler : public Sampler {
    public:
        explicit MetalSampler(MetalDevice &dev);
        ~MetalSampler() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return sampler; }

    private:
        MetalDevice &device;
        void        *sampler = nullptr;
    };

} // namespace sky::aurora