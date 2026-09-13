//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12RootSignature.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

namespace sky::aurora {

    static const char *TAG = "D3D12RootSignature";

    D3D12RootSignature::D3D12RootSignature(D3D12Device &dev)
        : device(dev)
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
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_DESCRIPTOR_RANGE_TYPE D3D12RootSignature::ToRangeType(ShaderResourceType type)
    {
        switch (type) {
        case ShaderResourceType::SAMPLER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case ShaderResourceType::SAMPLED_IMAGE:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case ShaderResourceType::STORAGE_IMAGE:
        case ShaderResourceType::STORAGE_BUFFER:
        case ShaderResourceType::STORAGE_BUFFER_DYNAMIC:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case ShaderResourceType::UNIFORM_BUFFER:
        case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC:
            // DYNAMIC types are handled as root CBV / root UAV in Init and
            // never reach this mapping; the fallthrough keeps static UBO as CBV.
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case ShaderResourceType::INPUT_ATTACHMENT:
        default:
            // D3D12 has no direct input attachment; treat as SRV.
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }
    }

    bool D3D12RootSignature::Init(const RootSignatureDescriptor &desc)
    {
        rangeSets.reserve(desc.sets.size() * 2);
        parameters.reserve(desc.sets.size() * 2 + desc.pushConstants.size());

        for (size_t i = 0; i < desc.sets.size(); ++i) {
            const uint32_t setIndex = i < desc.setIndices.size() ? desc.setIndices[i] : static_cast<uint32_t>(i);
            const auto    &set      = desc.sets[i];

            if (setParams.size() <= setIndex) {
                setParams.resize(setIndex + 1);
            }

            auto &cbvRanges = rangeSets.emplace_back();
            auto &smpRanges = rangeSets.emplace_back();
            cbvRanges.reserve(set.ranges.size());
            smpRanges.reserve(set.ranges.size());

            for (const auto &range : set.ranges) {
                if (range.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
                    range.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                    // Dynamic binding -> root CBV / root UAV. The root param
                    // holds a GPU virtual address; the per-draw offset is
                    // applied at bind time via SetGraphicsRoot*View.
                    D3D12_ROOT_PARAMETER param = {};
                    param.ParameterType             = range.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC
                                                          ? D3D12_ROOT_PARAMETER_TYPE_UAV
                                                          : D3D12_ROOT_PARAMETER_TYPE_CBV;
                    param.Descriptor.ShaderRegister = range.binding;
                    param.Descriptor.RegisterSpace  = setIndex;
                    param.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
                    setParams[setIndex].dynamicRootParams.emplace_back(range.binding, static_cast<uint32_t>(parameters.size()));
                    parameters.emplace_back(param);
                    continue;
                }

                D3D12_DESCRIPTOR_RANGE d3dRange = {};
                d3dRange.RangeType                         = ToRangeType(range.type);
                d3dRange.NumDescriptors                    = range.count;
                d3dRange.BaseShaderRegister                = range.binding;
                d3dRange.RegisterSpace                     = setIndex;
                d3dRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                if (range.type == ShaderResourceType::SAMPLER) {
                    smpRanges.push_back(d3dRange);
                } else {
                    cbvRanges.push_back(d3dRange);
                }
            }

            if (!cbvRanges.empty()) {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(cbvRanges.size());
                param.DescriptorTable.pDescriptorRanges   = cbvRanges.data();
                param.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
                setParams[setIndex].cbvSrvUav             = static_cast<uint32_t>(parameters.size());
                parameters.emplace_back(param);
            }

            if (!smpRanges.empty()) {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(smpRanges.size());
                param.DescriptorTable.pDescriptorRanges   = smpRanges.data();
                param.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
                setParams[setIndex].sampler               = static_cast<uint32_t>(parameters.size());
                parameters.emplace_back(param);
            }
        }

        // root constants (push constants)
        pushConstantRootParam = desc.pushConstants.empty() ? INVALID_INDEX : static_cast<uint32_t>(parameters.size());
        for (const auto &pc : desc.pushConstants) {
            D3D12_ROOT_PARAMETER param = {};
            param.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            param.Constants.ShaderRegister = 0;
            param.Constants.RegisterSpace  = 0;
            param.Constants.Num32BitValues = pc.size / 4;
            param.ShaderVisibility         = ToShaderVisibility(pc.stageFlags);
            parameters.emplace_back(param);
        }

        D3D12_ROOT_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.NumParameters     = static_cast<UINT>(parameters.size());
        signatureDesc.pParameters       = parameters.data();
        signatureDesc.NumStaticSamplers = 0;
        signatureDesc.pStaticSamplers   = nullptr;
        signatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(
            &signatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            signature.GetAddressOf(),
            error.GetAddressOf());

        if (FAILED(hr)) {
            if (error) {
                LOG_E(TAG, "serialize root signature failed: %s", static_cast<const char *>(error->GetBufferPointer()));
            }
            return false;
        }

        hr = device.GetNativeHandle()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(rootSignature.GetAddressOf()));

        if (FAILED(hr)) {
            LOG_E(TAG, "create root signature failed");
            return false;
        }

        return true;
    }

    uint32_t D3D12RootSignature::GetCbvSrvUavRootParam(uint32_t set) const
    {
        if (set < setParams.size()) {
            return setParams[set].cbvSrvUav;
        }
        return INVALID_INDEX;
    }

    uint32_t D3D12RootSignature::GetSamplerRootParam(uint32_t set) const
    {
        if (set < setParams.size()) {
            return setParams[set].sampler;
        }
        return INVALID_INDEX;
    }

    uint32_t D3D12RootSignature::GetDynamicRootParam(uint32_t set, uint32_t binding) const
    {
        if (set < setParams.size()) {
            for (const auto &kv : setParams[set].dynamicRootParams) {
                if (kv.first == binding) {
                    return kv.second;
                }
            }
        }
        return INVALID_INDEX;
    }

} // namespace sky::aurora
