//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>

#include <map>
#include <utility>

namespace sky::aurora {

    static const char *TAG = "D3D12RootSignature";

    namespace {
        DescriptorType FromShaderResourceType(ShaderResourceType type)
        {
            switch (type) {
            case ShaderResourceType::SAMPLER:          return DescriptorType::SAMPLER;
            case ShaderResourceType::SAMPLED_IMAGE:    return DescriptorType::SAMPLED_IMAGE;
            case ShaderResourceType::STORAGE_IMAGE:    return DescriptorType::STORAGE_IMAGE;
            case ShaderResourceType::UNIFORM_BUFFER:   return DescriptorType::UNIFORM_BUFFER;
            case ShaderResourceType::STORAGE_BUFFER:   return DescriptorType::STORAGE_BUFFER;
            case ShaderResourceType::INPUT_ATTACHMENT: return DescriptorType::INPUT_ATTACHMENT;
            }
            return DescriptorType::UNIFORM_BUFFER;
        }
    } // namespace

    D3D12ShaderFunction::D3D12ShaderFunction(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12ShaderFunction::Init(const Descriptor &desc)
    {
        if (desc.data == nullptr) {
            LOG_E(TAG, "shader function missing data provider");
            return false;
        }
        dataProvider = desc.data;
        return true;
    }

    D3D12_SHADER_BYTECODE D3D12ShaderFunction::GetByteCode() const
    {
        auto *binaryProvider = dynamic_cast<ShaderBinaryProvider *>(dataProvider.Get());
        if (binaryProvider == nullptr || binaryProvider->binaryData == nullptr) {
            return {};
        }
        const auto &binData = binaryProvider->binaryData;
        D3D12_SHADER_BYTECODE bytecode = {};
        bytecode.pShaderBytecode = binData->Data();
        bytecode.BytecodeLength  = binData->Size();
        return bytecode;
    }

    D3D12Shader::D3D12Shader(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12Shader::Init(const Descriptor &desc)
    {
        if (desc.vs == nullptr && desc.ps == nullptr && desc.cs == nullptr) {
            LOG_E(TAG, "shader missing all shader stages");
            return false;
        }
        vs = desc.vs;
        // ps and cs are mutually exclusive; cs aliases vs in the descriptor union
        psOrCs = desc.ps != nullptr ? desc.ps : desc.cs;

        // build root signature directly from shader reflection
        RootSignatureDescriptor rsDesc{};
        if (desc.reflection != nullptr) {
            std::map<uint32_t, RootSignatureDescriptorSet> sets;
            for (const auto &res : desc.reflection->resources) {
                RootSignatureDescriptorRange range{};
                range.type       = FromShaderResourceType(res.type);
                range.binding    = res.binding;
                range.count      = res.count;
                range.visibility = ShaderStageFlagBit::GFX;
                sets[res.set].ranges.push_back(range);
            }
            for (auto &entry : sets) {
                rsDesc.sets.push_back(std::move(entry.second));
            }
            rsDesc.pushConstants = desc.reflection->pushConstants;
        }

        rootSignature = new D3D12RootSignature(device);
        return rootSignature->Init(rsDesc);
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetVSByteCode() const
    {
        if (vs == nullptr) return {};
        return static_cast<D3D12ShaderFunction *>(vs.Get())->GetByteCode();
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetPSByteCode() const
    {
        if (psOrCs == nullptr) return {};
        return static_cast<D3D12ShaderFunction *>(psOrCs.Get())->GetByteCode();
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetCSByteCode() const
    {
        if (psOrCs == nullptr) return {};
        return static_cast<D3D12ShaderFunction *>(psOrCs.Get())->GetByteCode();
    }

} // namespace sky::aurora