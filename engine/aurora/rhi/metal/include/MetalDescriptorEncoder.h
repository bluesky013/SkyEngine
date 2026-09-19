//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorEncoder.h>
#include <MetalResourceGroup.h>
#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalSampler.h>

namespace sky::aurora {

    // Metal descriptor writes are immediate table updates on the target
    // ResourceGroup (direct binding model, no native descriptor objects).
    class MetalDescriptorEncoder : public DescriptorEncoder {
    public:
        explicit MetalDescriptorEncoder(MetalResourceGroup &group) : group(&group) {}
        ~MetalDescriptorEncoder() override = default;

        void WriteBuffer(uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t /*range*/,
                         uint32_t /*arrayElement*/ = 0) override
        {
            group->WriteBuffer(binding, static_cast<MetalBuffer *>(buffer), offset);
        }
        void WriteImage(uint32_t binding, Image *image,
                        ImageLayout /*layout*/, uint32_t /*arrayElement*/ = 0) override
        {
            group->WriteImage(binding, static_cast<MetalImage *>(image));
        }
        void WriteSampler(uint32_t binding, Sampler *sampler,
                          uint32_t /*arrayElement*/ = 0) override
        {
            group->WriteSampler(binding, static_cast<MetalSampler *>(sampler));
        }
        void End() override {}

    private:
        MetalResourceGroup *group;
    };

} // namespace sky::aurora
