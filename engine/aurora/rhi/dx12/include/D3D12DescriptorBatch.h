//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorBatch.h>

namespace sky::aurora {

    class D3D12Device;

    // Immediate group writes retain the first failure until Reset().
    class D3D12DescriptorBatch : public DescriptorBatch {
    public:
        explicit D3D12DescriptorBatch(D3D12Device &dev);
        ~D3D12DescriptorBatch() override = default;

        bool WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override;

        bool Flush() override
        {
            return !failed;
        }
        void Reset() override
        {
            failed = false;
        }

    private:
        D3D12Device *device = nullptr;
        bool         failed = false;
    };

} // namespace sky::aurora
