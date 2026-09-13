//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorEncoder.h>

namespace sky::aurora {

    // Metal ResourceGroup does not exist yet (aurora-resource-group Metal
    // phase). This stub keeps the DescriptorEncoder interface in place for when
    // MetalResourceGroup lands; argument buffer writes will be implemented then.
    class MetalDescriptorEncoder : public DescriptorEncoder {
    public:
        MetalDescriptorEncoder()           = default;
        ~MetalDescriptorEncoder() override = default;

        void WriteBuffer(uint32_t /*binding*/, Buffer * /*buffer*/,
                         uint64_t /*offset*/, uint64_t /*range*/,
                         uint32_t /*arrayElement*/ = 0) override
        {
        }
        void WriteImage(uint32_t /*binding*/, Image * /*image*/,
                        ImageLayout /*layout*/, uint32_t /*arrayElement*/ = 0) override
        {
        }
        void WriteSampler(uint32_t /*binding*/, Sampler * /*sampler*/,
                          uint32_t /*arrayElement*/ = 0) override
        {
        }
        void End() override {}
    };

} // namespace sky::aurora
