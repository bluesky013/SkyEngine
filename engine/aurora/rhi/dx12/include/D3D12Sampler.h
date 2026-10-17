//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Sampler.h>

#include <d3d12.h>

namespace sky::aurora {

    class D3D12Device;

    // Retains the native descriptor until it is written into a sampler heap.
    class D3D12Sampler : public Sampler {
    public:
        explicit D3D12Sampler(D3D12Device &dev);
        ~D3D12Sampler() override = default;
        const Device *GetDevice() const noexcept override;

        bool Init(const Descriptor &desc);

        const D3D12_SAMPLER_DESC &GetNativeDesc() const
        {
            return samplerDesc;
        }

    private:
        D3D12Device       &device;
        D3D12_SAMPLER_DESC samplerDesc = {};
    };

} // namespace sky::aurora
