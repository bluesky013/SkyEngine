//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    struct RootSignatureDescriptorRange {
        DescriptorType type    = DescriptorType::UNIFORM_BUFFER;
        uint32_t       binding = 0;
        uint32_t       count   = 1;
        ShaderStageFlags visibility;
    };

    struct RootSignatureDescriptorSet {
        std::vector<RootSignatureDescriptorRange> ranges;
    };

    struct RootSignatureDescriptor {
        std::vector<RootSignatureDescriptorSet> sets;
        std::vector<PushConstantRange>          pushConstants;
    };

    class D3D12RootSignature : public RefObject {
    public:
        explicit D3D12RootSignature(D3D12Device &dev);
        ~D3D12RootSignature() override = default;

        bool Init(const RootSignatureDescriptor &desc);

        ID3D12RootSignature *GetNativeHandle() const { return rootSignature.Get(); }

    private:
        static D3D12_SHADER_VISIBILITY ToShaderVisibility(ShaderStageFlags flags);
        static D3D12_DESCRIPTOR_RANGE_TYPE ToRangeType(DescriptorType type);

        D3D12Device &device;
        ComPtr<ID3D12RootSignature> rootSignature;

        // keep alive for the lifetime of the root signature
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangeSets;
        std::vector<D3D12_ROOT_PARAMETER>                parameters;
    };

} // namespace sky::aurora