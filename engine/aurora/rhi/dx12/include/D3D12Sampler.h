//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Sampler.h>

#include <d3d12.h>

namespace sky::aurora {

    class D3D12Device;

    // D3D12 has no standalone sampler object. Instead, a sampler descriptor
    // is written into a D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER heap. This class
    // caches the D3D12_SAMPLER_DESC so it can be written on demand.
    class D3D12Sampler : public Sampler {
    public:
        explicit D3D12Sampler(D3D12Device &dev);
        ~D3D12Sampler() override = default;

        bool Init(const Descriptor &desc);

        const D3D12_SAMPLER_DESC &GetNativeDesc() const { return samplerDesc; }

    private:
        D3D12Device      &device;
        D3D12_SAMPLER_DESC samplerDesc = {};
    };

} // namespace sky::aurora
