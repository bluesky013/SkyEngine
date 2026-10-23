//
// Created on 2026/09/14.
//

#pragma once

#include <D3D12ResourceGroup.h>
#include <aurora/rhi/DescriptorEncoder.h>

namespace sky::aurora {

    class D3D12ResourceGroup;

    // Writes target a candidate snapshot; any rejected write keeps End() failed.
    class D3D12DescriptorEncoder : public DescriptorEncoder {
    public:
        explicit D3D12DescriptorEncoder(D3D12ResourceGroup &group);
        ~D3D12DescriptorEncoder() override = default;

        bool WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override;
        bool End() override
        {
            return !failed;
        }

    private:
        bool                           Fail(const char *message);
        uint32_t                       FindBinding(uint32_t binding) const;
        CounterPtr<D3D12ResourceGroup> group;
        bool                           failed = false;
    };

} // namespace sky::aurora
