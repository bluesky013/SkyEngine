//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorBatch.h>

namespace sky::aurora {

    class D3D12Device;

    // Thin cross-set batch: DX12 descriptor writes are immediate (CPU staging
    // heap + shader-visible ring copy at bind), so Write* delegates to the
    // per-group encoder and Flush()/Reset() are no-ops.
    class D3D12DescriptorBatch : public DescriptorBatch {
    public:
        explicit D3D12DescriptorBatch(D3D12Device &dev);
        ~D3D12DescriptorBatch() override = default;

        void WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        void WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                        ImageLayout layout, uint32_t arrayElement = 0) override;
        void WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler,
                          uint32_t arrayElement = 0) override;

        void Flush() override {}
        void Reset() override {}

    private:
        D3D12Device *device = nullptr;
    };

} // namespace sky::aurora
