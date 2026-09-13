//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderReflection.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <utility>
#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    struct RootSignatureDescriptorRange {
        ShaderResourceType type    = ShaderResourceType::UNIFORM_BUFFER;
        uint32_t           binding = 0;
        uint32_t           count   = 1;
        ShaderStageFlags   visibility;
    };

    struct RootSignatureDescriptorSet {
        std::vector<RootSignatureDescriptorRange> ranges;
    };

    struct RootSignatureDescriptor {
        std::vector<RootSignatureDescriptorSet> sets;
        std::vector<uint32_t>                  setIndices;   // original set index per sets[i]
        std::vector<PushConstantRange>         pushConstants;
    };

    class D3D12RootSignature : public RefObject {
    public:
        explicit D3D12RootSignature(D3D12Device &dev);
        ~D3D12RootSignature() override = default;

        bool Init(const RootSignatureDescriptor &desc);

        ID3D12RootSignature *GetNativeHandle() const { return rootSignature.Get(); }

        // Root param index of the CBV/SRV/UAV descriptor table for `set`,
        // or INVALID_INDEX when the set has no non-sampler bindings.
        uint32_t GetCbvSrvUavRootParam(uint32_t set) const;
        // Root param index of the sampler descriptor table for `set`,
        // or INVALID_INDEX when the set has no sampler bindings.
        uint32_t GetSamplerRootParam(uint32_t set) const;
        // Root param index of the root CBV / root UAV for a dynamic binding,
        // or INVALID_INDEX when the set has no such binding.
        uint32_t GetDynamicRootParam(uint32_t set, uint32_t binding) const;
        // Root param index of the first push-constant root param.
        uint32_t GetPushConstantRootParam() const { return pushConstantRootParam; }

    private:
        static D3D12_SHADER_VISIBILITY ToShaderVisibility(ShaderStageFlags flags);
        static D3D12_DESCRIPTOR_RANGE_TYPE ToRangeType(ShaderResourceType type);

        D3D12Device &device;
        ComPtr<ID3D12RootSignature> rootSignature;

        // keep alive for the lifetime of the root signature
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangeSets;
        std::vector<D3D12_ROOT_PARAMETER>                parameters;

        struct SetRootParams {
            uint32_t cbvSrvUav = INVALID_INDEX;
            uint32_t sampler   = INVALID_INDEX;
            // dynamic bindings -> root CBV / root UAV param (binding, rootParam)
            std::vector<std::pair<uint32_t, uint32_t>> dynamicRootParams;
        };
        std::vector<SetRootParams> setParams; // indexed by set index
        uint32_t                   pushConstantRootParam = INVALID_INDEX;
    };

} // namespace sky::aurora
