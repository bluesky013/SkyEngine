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

        void *GetNativeHandle() const
        {
            return buffer;
        }

        const Device *GetDevice() const noexcept override;
        uint64_t GetSize() const override
        {
            return size;
        }

        uint8_t *Map() override;
        void     UnMap() override
        {
        }

    private:
        MetalDevice &device;
        void        *buffer = nullptr;
        uint64_t     size   = 0;
    };

} // namespace sky::aurora