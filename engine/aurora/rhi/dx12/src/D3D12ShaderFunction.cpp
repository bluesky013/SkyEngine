//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>

namespace sky::aurora {

    static const char *TAG = "D3D12RootSignature";

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
        psOrCs = desc.ps;
        return true;
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