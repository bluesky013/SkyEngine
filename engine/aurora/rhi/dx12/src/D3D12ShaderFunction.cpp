//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12Device.h>
#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>

#include <map>
#include <utility>

namespace sky::aurora {
    const Device *D3D12ShaderFunction::GetDevice() const noexcept
    {
        return &device;
    }
    const Device *D3D12Shader::GetDevice() const noexcept
    {
        return &device;
    }

    static const char *TAG = "D3D12RootSignature";

    D3D12ShaderFunction::D3D12ShaderFunction(D3D12Device &dev) : device(dev)
    {
    }

    bool D3D12ShaderFunction::Init(const Descriptor &desc)
    {
        if (desc.data == nullptr || !desc.data->GetBinaryData() || !desc.data->GetBinaryData()->Size()) {
            LOG_E(TAG, "shader function missing data provider");
            return false;
        }
        stage        = desc.stage;
        dataProvider = desc.data;
        return true;
    }

    D3D12_SHADER_BYTECODE D3D12ShaderFunction::GetByteCode() const
    {
        auto *binData = dataProvider ? dataProvider->GetBinaryData() : nullptr;
        if (!binData) {
            return {};
        }
        D3D12_SHADER_BYTECODE bytecode = {};
        bytecode.pShaderBytecode       = binData->Data();
        bytecode.BytecodeLength        = binData->Size();
        return bytecode;
    }

    D3D12Shader::D3D12Shader(D3D12Device &dev) : device(dev)
    {
    }

    bool D3D12Shader::Init(const Descriptor &desc)
    {
        if ((desc.vs && desc.vs->GetDevice() != &device) || (desc.ps && desc.ps->GetDevice() != &device))
            return false;
        if (desc.vs == nullptr && desc.ps == nullptr && desc.cs == nullptr) {
            LOG_E(TAG, "shader missing all shader stages");
            return false;
        }
        vs = desc.vs;
        // ps and cs are mutually exclusive; cs aliases vs in the descriptor union
        psOrCs = desc.ps != nullptr ? desc.ps : desc.cs;

        if (desc.reflection == nullptr) {
            LOG_E(TAG, "shader requires a non-null reflection");
            return false;
        }
        reflection = *desc.reflection;

        // build root signature directly from shader reflection
        RootSignatureDescriptor                        rsDesc{};
        std::map<uint32_t, RootSignatureDescriptorSet> sets;
        for (const auto &res : reflection.resources) {
            RootSignatureDescriptorRange range{};
            range.type       = res.type;
            range.binding    = res.binding;
            range.count      = res.count;
            range.visibility = ShaderStageFlagBit::GFX;
            sets[res.set].ranges.push_back(range);
        }
        for (auto &entry : sets) {
            rsDesc.setIndices.push_back(entry.first);
            rsDesc.sets.push_back(std::move(entry.second));
        }
        rsDesc.pushConstants = reflection.pushConstants;

        rootSignature = new D3D12RootSignature(device);
        return rootSignature->Init(rsDesc);
    }

    bool D3D12Shader::IsGraphics() const noexcept
    {
        return vs && psOrCs && static_cast<D3D12ShaderFunction *>(vs.Get())->GetStage() == ShaderStageFlagBit::VS &&
               static_cast<D3D12ShaderFunction *>(psOrCs.Get())->GetStage() == ShaderStageFlagBit::FS;
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetVSByteCode() const
    {
        if (vs == nullptr)
            return {};
        return static_cast<D3D12ShaderFunction *>(vs.Get())->GetByteCode();
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetPSByteCode() const
    {
        if (psOrCs == nullptr)
            return {};
        return static_cast<D3D12ShaderFunction *>(psOrCs.Get())->GetByteCode();
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetCSByteCode() const
    {
        if (psOrCs == nullptr)
            return {};
        return static_cast<D3D12ShaderFunction *>(psOrCs.Get())->GetByteCode();
    }

} // namespace sky::aurora
