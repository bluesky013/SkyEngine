//
// Created on 2026/04/02.
//

#include "D3D12Sampler.h"
#include "D3D12Device.h"
#include "D3D12Conversion.h"

namespace sky::aurora {

    D3D12Sampler::D3D12Sampler(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12Sampler::Init(const Descriptor &desc)
    {
        samplerDesc.Filter   = FromFilter(desc.minFilter, desc.magFilter, desc.mipmapMode, desc.anisotropyEnable);
        samplerDesc.AddressU = FromWrapMode(desc.addressModeU);
        samplerDesc.AddressV = FromWrapMode(desc.addressModeV);
        samplerDesc.AddressW = FromWrapMode(desc.addressModeW);
        samplerDesc.MipLODBias    = 0.f;
        samplerDesc.MaxAnisotropy = static_cast<UINT>(desc.maxAnisotropy);
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor[0] = 0.f;
        samplerDesc.BorderColor[1] = 0.f;
        samplerDesc.BorderColor[2] = 0.f;
        samplerDesc.BorderColor[3] = 0.f;
        samplerDesc.MinLOD = desc.minLod;
        samplerDesc.MaxLOD = desc.maxLod;

        return true;
    }

} // namespace sky::aurora
