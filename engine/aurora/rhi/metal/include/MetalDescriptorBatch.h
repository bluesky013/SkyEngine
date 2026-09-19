//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorBatch.h>
#include <MetalResourceGroup.h>
#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalSampler.h>

namespace sky::aurora {

    // Metal descriptor writes are immediate (no vkUpdateDescriptorSets-style
    // batching exists); Flush/Reset are therefore no-ops.
    class MetalDescriptorBatch : public DescriptorBatch {
    public:
        MetalDescriptorBatch()           = default;
        ~MetalDescriptorBatch() override = default;

        void WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t /*range*/, uint32_t /*arrayElement*/ = 0) override
        {
            static_cast<MetalResourceGroup *>(group)->WriteBuffer(binding, static_cast<MetalBuffer *>(buffer), offset);
        }
        void WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                        ImageLayout /*layout*/, uint32_t /*arrayElement*/ = 0) override
        {
            static_cast<MetalResourceGroup *>(group)->WriteImage(binding, static_cast<MetalImage *>(image));
        }
        void WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler,
                          uint32_t /*arrayElement*/ = 0) override
        {
            static_cast<MetalResourceGroup *>(group)->WriteSampler(binding, static_cast<MetalSampler *>(sampler));
        }
        void Flush() override {}
        void Reset() override {}
    };

} // namespace sky::aurora
