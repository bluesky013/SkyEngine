//
// Created on 2026/09/13.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <vector>

namespace sky::aurora {

    class D3D12Device;

    // D3D12 descriptor layout: binds a set of (binding -> descriptor type)
    // entries to a CBV/SRV/UAV + sampler descriptor allocation. Combined image
    // samplers are split into an SRV slot plus a sampler slot.
    class D3D12ResourceGroupLayout : public ResourceGroupLayout {
    public:
        explicit D3D12ResourceGroupLayout(D3D12Device &dev);
        ~D3D12ResourceGroupLayout() override = default;

        bool Init(const Descriptor &desc);

        const std::vector<BindingDesc> &GetBindings() const { return bindings; }
        uint32_t                       GetCbvSrvUavCount() const { return cbvSrvUavCount; }
        uint32_t                       GetSamplerCount() const { return samplerCount; }

        // Returns the bindings array index for a binding number, or INVALID_INDEX.
        uint32_t FindBinding(uint32_t binding) const;
        uint32_t GetCbvSrvUavOffset(uint32_t index) const { return cbvSrvUavOffsets[index]; }
        uint32_t GetSamplerOffset(uint32_t index) const { return samplerOffsets[index]; }

    private:
        D3D12Device           &device;
        std::vector<BindingDesc> bindings;
        std::vector<uint32_t> cbvSrvUavOffsets;
        std::vector<uint32_t> samplerOffsets;
        uint32_t              cbvSrvUavCount = 0;
        uint32_t              samplerCount   = 0;
    };

} // namespace sky::aurora
