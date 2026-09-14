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
    class ResourceGroup;

    // DescriptorBatch: frame-scoped cross-set descriptor write accumulator.
    // Write* targets any ResourceGroup; Flush() commits the whole frame once;
    // Reset() clears for reuse next frame. Backends implement natively
    // (Vulkan: single vkUpdateDescriptorSets; DX12/Metal: immediate writes).
    class DescriptorBatch {
    public:
        virtual ~DescriptorBatch() = default;

        virtual void WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                                 uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
        virtual void WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                                ImageLayout layout, uint32_t arrayElement = 0) = 0;
        virtual void WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler,
                                  uint32_t arrayElement = 0) = 0;

        virtual void Flush() = 0;
        virtual void Reset() = 0;
    };

} // namespace sky::aurora
