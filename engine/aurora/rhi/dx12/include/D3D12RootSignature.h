//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderReflection.h>
#include <d3d12.h>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    struct RootSignatureDescriptorRange {
        ShaderResourceType   type    = ShaderResourceType::UNIFORM_BUFFER;
        uint32_t             binding = 0;
        uint32_t             count   = 1;
        ShaderStageFlags     visibility;
        ShaderResourceAccess access         = ShaderResourceAccess::READ_ONLY;
        ShaderNativeCategory category       = ShaderNativeCategory::UNKNOWN;
        uint32_t             nativeRegister = 0, nativeSpace = 0, descriptorOffset = 0;
    };

    struct RootSignatureDescriptorSet {
        std::vector<RootSignatureDescriptorRange> ranges;
    };

    struct RootSignatureDescriptor {
        std::vector<RootSignatureDescriptorSet> sets;
        std::vector<uint32_t>                   setIndices; // original set index per sets[i]
        std::vector<PushConstantRange>          pushConstants;
    };

    struct D3D12RootTableBinding {
        uint32_t root    = 0;
        bool     sampler = false;
    };
    struct D3D12DynamicRootBinding {
        uint32_t             binding = 0, root = 0;
        ShaderNativeCategory category = ShaderNativeCategory::UNKNOWN;
    };

    class D3D12RootSignature : public RefObject {
    public:
        explicit D3D12RootSignature(D3D12Device &dev);
        ~D3D12RootSignature() override = default;

        bool Init(const RootSignatureDescriptor &desc);

        ID3D12RootSignature *GetNativeHandle() const
        {
            return rootSignature.Get();
        }

        std::span<const D3D12RootTableBinding>   GetDescriptorTables(uint32_t set) const;
        std::span<const D3D12DynamicRootBinding> GetDynamicBindings(uint32_t set) const;
        // Root param index of the first push-constant root param.
        uint32_t GetPushConstantRootParam() const
        {
            return pushConstantRootParam;
        }

    private:
        static D3D12_SHADER_VISIBILITY                    ToShaderVisibility(ShaderStageFlags flags);
        static std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> ToRangeType(ShaderNativeCategory category);

        D3D12Device                &device;
        ComPtr<ID3D12RootSignature> rootSignature;

        // keep alive for the lifetime of the root signature
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangeSets;
        std::vector<D3D12_ROOT_PARAMETER>                parameters;

        struct SetRootParams {
            std::vector<D3D12RootTableBinding>   tables;
            std::vector<D3D12DynamicRootBinding> dynamicRootParams;
        };
        std::vector<SetRootParams> setParams; // indexed by set index
        uint32_t                   pushConstantRootParam = INVALID_INDEX;
    };

} // namespace sky::aurora
