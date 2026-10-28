//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/Core.h>

#include <cstdint>

namespace sky::aurora {

    class Buffer;
    class Image;
    class Sampler;
    // Native writes require a mutable group whose prior GPU uses have retired.
    class DescriptorEncoder {
    public:
        virtual ~DescriptorEncoder()                                                                                           = default;
        virtual bool WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
        virtual bool WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)                 = 0;
        virtual bool WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)                               = 0;
        virtual bool End()                                                                                                     = 0;
    };

} // namespace sky::aurora
