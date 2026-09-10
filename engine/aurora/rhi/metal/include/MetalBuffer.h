//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Buffer.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalBuffer : public Buffer {
    public:
        explicit MetalBuffer(MetalDevice &dev);
        ~MetalBuffer() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return buffer; }

        uint8_t *Map() override;
        void UnMap() override {}

    private:
        MetalDevice &device;
        void        *buffer = nullptr;
    };

} // namespace sky::aurora