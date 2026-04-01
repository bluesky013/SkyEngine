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

    D3D12_DESCRIPTOR_RANGE_TYPE D3D12RootSignature::ToRangeType(DescriptorType type)
    {
        switch (type) {
        case DescriptorType::SAMPLER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case DescriptorType::SAMPLED_IMAGE:
        case DescriptorType::COMBINED_IMAGE_SAMPLER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::STORAGE_IMAGE:
        case DescriptorType::STORAGE_BUFFER:
        case DescriptorType::STORAGE_BUFFER_DYNAMIC:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case DescriptorType::UNIFORM_BUFFER:
        case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case DescriptorType::INPUT_ATTACHMENT:
            // D3D12 doesn't have a direct equivalent for input attachments, treat them as SRV
        default:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }
    }

    bool D3D12RootSignature::Init(const RootSignatureDescriptor &desc)
    {
        rangeSets.reserve(desc.sets.size());
        parameters.reserve(desc.sets.size() + desc.pushConstants.size());

        // descriptor table parameters
        for (UINT i = 0; i < desc.sets.size(); ++i) {
            const auto &set = desc.sets[i];

            auto &ranges = rangeSets.emplace_back();
            ranges.reserve(set.ranges.size());

            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;

            for (const auto &range : set.ranges) {
                D3D12_DESCRIPTOR_RANGE d3dRange = {};
                d3dRange.RangeType                         = ToRangeType(range.type);
                d3dRange.NumDescriptors                    = range.count;
                d3dRange.BaseShaderRegister                = range.binding;
                d3dRange.RegisterSpace                     = i;
                d3dRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                ranges.emplace_back(d3dRange);

                visibility = ToShaderVisibility(range.visibility);
            }

            D3D12_ROOT_PARAMETER param = {};
            param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
            param.DescriptorTable.pDescriptorRanges   = ranges.data();
            param.ShaderVisibility                    = visibility;
            parameters.emplace_back(param);
        }

        // root constants (push constants)
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

} // namespace sky::aurora