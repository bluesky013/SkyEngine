//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorEncoder.h>

namespace sky::aurora {

    class D3D12ResourceGroup;

    // D3D12 has no batched descriptor flush: static bindings are written
    // immediately into the descriptor heap; dynamic bindings are recorded for
    // root CBV/UAV rebind at bind time. End() is a no-op (interface symmetry).
    class D3D12DescriptorEncoder : public DescriptorEncoder {
    public:
        explicit D3D12DescriptorEncoder(D3D12ResourceGroup &group);
        ~D3D12DescriptorEncoder() override = default;

        void WriteBuffer(uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t range,
                         uint32_t arrayElement = 0) override;
        void WriteImage(uint32_t binding, Image *image,
                        ImageLayout layout,
                        uint32_t arrayElement = 0) override;
        void WriteSampler(uint32_t binding, Sampler *sampler,
                          uint32_t arrayElement = 0) override;
        void End() override {}

    private:
        D3D12ResourceGroup *group;
    };

} // namespace sky::aurora
