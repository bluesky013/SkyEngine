//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorBatch.h>

namespace sky::aurora {

    // Metal ResourceGroup does not exist yet (aurora-resource-group Metal
    // phase). Stub keeps the DescriptorBatch interface in place; argument
    // buffer writes will be implemented then.
    class MetalDescriptorBatch : public DescriptorBatch {
    public:
        MetalDescriptorBatch()           = default;
        ~MetalDescriptorBatch() override = default;

        void WriteBuffer(ResourceGroup * /*group*/, uint32_t /*binding*/, Buffer * /*buffer*/,
                         uint64_t /*offset*/, uint64_t /*range*/, uint32_t /*arrayElement*/ = 0) override
        {
        }
        void WriteImage(ResourceGroup * /*group*/, uint32_t /*binding*/, Image * /*image*/,
                        ImageLayout /*layout*/, uint32_t /*arrayElement*/ = 0) override
        {
        }
        void WriteSampler(ResourceGroup * /*group*/, uint32_t /*binding*/, Sampler * /*sampler*/,
                          uint32_t /*arrayElement*/ = 0) override
        {
        }
        void Flush() override {}
        void Reset() override {}
    };

} // namespace sky::aurora
