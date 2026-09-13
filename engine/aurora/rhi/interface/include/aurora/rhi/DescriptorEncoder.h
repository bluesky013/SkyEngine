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

    // DescriptorEncoder: type-safe batched descriptor set writes. Created from a
    // ResourceGroup, Write* accumulates native writes into the group, End()
    // commits. Backends implement it natively (no generic intermediate struct).
    class DescriptorEncoder {
    public:
        virtual ~DescriptorEncoder() = default;

        virtual void WriteBuffer(uint32_t binding, Buffer *buffer,
                                 uint64_t offset, uint64_t range,
                                 uint32_t arrayElement = 0) = 0;
        virtual void WriteImage(uint32_t binding, Image *image,
                                ImageLayout layout,
                                uint32_t arrayElement = 0) = 0;
        virtual void WriteSampler(uint32_t binding, Sampler *sampler,
                                  uint32_t arrayElement = 0) = 0;
        virtual void End() = 0;
    };

} // namespace sky::aurora
