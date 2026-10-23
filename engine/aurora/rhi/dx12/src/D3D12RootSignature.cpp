//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12RootSignature.h>

#include <D3D12Device.h>
#include <core/logger/Logger.h>

#include <map>

namespace sky::aurora {

    static const char *TAG = "D3D12RootSignature";

    D3D12RootSignature::D3D12RootSignature(D3D12Device &dev) : device(dev)
    {
    }

    D3D12_SHADER_VISIBILITY D3D12RootSignature::ToShaderVisibility(ShaderStageFlags flags)
    {
        if (flags == ShaderStageFlagBit::VS) {
            return D3D12_SHADER_VISIBILITY_VERTEX;
        }
        if (flags == ShaderStageFlagBit::FS) {
            return D3D12_SHADER_VISIBILITY_PIXEL;
        }
        if (flags == ShaderStageFlagBit::TAS)
            return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        if (flags == ShaderStageFlagBit::MS)
            return D3D12_SHADER_VISIBILITY_MESH;
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> D3D12RootSignature::ToRangeType(ShaderNativeCategory category)
    {
        switch (category) {
        case ShaderNativeCategory::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case ShaderNativeCategory::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case ShaderNativeCategory::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case ShaderNativeCategory::SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        default: return {};
        }
    }

    bool D3D12RootSignature::Init(const RootSignatureDescriptor &desc)
    {
        if (rootSignature || desc.sets.size() != desc.setIndices.size() || !desc.pushConstants.empty())
            return false;
        rangeSets.clear();
        parameters.clear();
        setParams.clear();
        uint32_t rootCost = 0;
        for (size_t i = 0; i < desc.sets.size(); ++i) {
            const auto setIndex = desc.setIndices[i];
            if (setIndex >= 64) {
                LOG_E(TAG, "Logical set exceeds the DX12 root plan budget");
                return false;
            }
            if (setParams.size() <= setIndex)
                setParams.resize(setIndex + 1);
            const auto                                                                                           &set = desc.sets[i];
            std::map<std::pair<D3D12_SHADER_VISIBILITY, bool>, std::vector<const RootSignatureDescriptorRange *>> tables;
            for (const auto &range : set.ranges) {
                const auto nativeType = ToRangeType(range.category);
                if (!nativeType || !range.count || range.count > UINT32_MAX - range.nativeRegister ||
                    ((range.type == ShaderResourceType::STORAGE_BUFFER || range.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) &&
                     range.access != ShaderResourceAccess::READ_ONLY && range.access != ShaderResourceAccess::READ_WRITE)) {
                    LOG_E(TAG, "Unknown or invalid DX12 native resource range");
                    return false;
                }
                const auto visibility = ToShaderVisibility(range.visibility);
                if (range.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || range.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                    if (range.count != 1 || range.category == ShaderNativeCategory::SAMPLER || rootCost > 62) {
                        LOG_E(TAG, "Unsupported dynamic root descriptor or root budget exceeded");
                        return false;
                    }
                    D3D12_ROOT_PARAMETER param{};
                    param.ParameterType             = range.category == ShaderNativeCategory::CBV   ? D3D12_ROOT_PARAMETER_TYPE_CBV
                                                      : range.category == ShaderNativeCategory::SRV ? D3D12_ROOT_PARAMETER_TYPE_SRV
                                                                                                    : D3D12_ROOT_PARAMETER_TYPE_UAV;
                    param.Descriptor.ShaderRegister = range.nativeRegister;
                    param.Descriptor.RegisterSpace  = range.nativeSpace;
                    param.ShaderVisibility          = visibility;
                    setParams[setIndex].dynamicRootParams.push_back({range.binding, static_cast<uint32_t>(parameters.size()), range.category});
                    parameters.push_back(param);
                    rootCost += 2;
                } else {
                    if (range.count > UINT32_MAX - range.descriptorOffset)
                        return false;
                    tables[{visibility, range.category == ShaderNativeCategory::SAMPLER}].push_back(&range);
                }
            }
            for (const auto &[key, entries] : tables) {
                if (rootCost >= 64) {
                    LOG_E(TAG, "DX12 root descriptor table budget exceeded");
                    return false;
                }
                auto &ranges = rangeSets.emplace_back();
                ranges.reserve(entries.size());
                for (const auto *range : entries) {
                    D3D12_DESCRIPTOR_RANGE native{};
                    native.RangeType          = *ToRangeType(range->category);
                    native.NumDescriptors     = range->count;
                    native.BaseShaderRegister = range->nativeRegister;
                    native.RegisterSpace      = range->nativeSpace;
                    // Stage tables share the logical descriptor snapshot while using distinct native addresses.
                    native.OffsetInDescriptorsFromTableStart = range->descriptorOffset;
                    ranges.push_back(native);
                }
                D3D12_ROOT_PARAMETER param{};
                param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
                param.DescriptorTable.pDescriptorRanges   = ranges.data();
                param.ShaderVisibility                    = key.first;
                const auto root                           = static_cast<uint32_t>(parameters.size());
                auto      &mapping                        = setParams[setIndex];
                mapping.tables.push_back({root, key.second});
                parameters.push_back(param);
                ++rootCost;
            }
        }

        D3D12_ROOT_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.NumParameters             = static_cast<UINT>(parameters.size());
        signatureDesc.pParameters               = parameters.data();
        signatureDesc.NumStaticSamplers         = 0;
        signatureDesc.pStaticSamplers           = nullptr;
        signatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf());

        if (FAILED(hr)) {
            if (error) {
                LOG_E(TAG, "serialize root signature failed: %s", static_cast<const char *>(error->GetBufferPointer()));
            }
            return false;
        }

        hr = device.GetNativeHandle()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                           IID_PPV_ARGS(rootSignature.GetAddressOf()));

        if (FAILED(hr)) {
            LOG_E(TAG, "create root signature failed");
            return false;
        }

        return true;
    }

    std::span<const D3D12RootTableBinding> D3D12RootSignature::GetDescriptorTables(uint32_t set) const
    {
        return set < setParams.size() ? std::span<const D3D12RootTableBinding>(setParams[set].tables) : std::span<const D3D12RootTableBinding>{};
    }

    std::span<const D3D12DynamicRootBinding> D3D12RootSignature::GetDynamicBindings(uint32_t set) const
    {
        return set < setParams.size() ? std::span<const D3D12DynamicRootBinding>(setParams[set].dynamicRootParams)
                                      : std::span<const D3D12DynamicRootBinding>{};
    }

} // namespace sky::aurora
