//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/SwapChain.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalSwapChain : public SwapChain {
    public:
        explicit MetalSwapChain(MetalDevice &dev);
        ~MetalSwapChain() override;

        bool Init(const Descriptor &desc);

        void *GetLayer() const { return layer; }

    private:
        MetalDevice &device;
        void        *layer = nullptr;
    };

} // namespace sky::aurora