//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12ShaderFunction.h>

#include <D3D12Device.h>
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
        std::string message;
        if (!InitArtifact(desc, API::DX12, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_5};
        auto                           *native = device.GetNativeHandle();
        if (!native || FAILED(native->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model))) ||
            model.HighestShaderModel < D3D_SHADER_MODEL_6_5) {
            LOG_E(TAG, "DXIL baseline requires device Shader Model 6.5 support");
            return false;
        }
        stage = desc.stage;
        return true;
    }

    D3D12_SHADER_BYTECODE D3D12ShaderFunction::GetByteCode() const
    {
        if (!artifact)
            return {};
        D3D12_SHADER_BYTECODE bytecode = {};
        bytecode.pShaderBytecode       = artifact->data.data();
        bytecode.BytecodeLength        = artifact->data.size();
        return bytecode;
    }

    D3D12Shader::D3D12Shader(D3D12Device &dev) : device(dev)
    {
    }

    bool D3D12Shader::Init(const Descriptor &desc)
    {
        std::string message;
        if (!BuildShaderProgramReflection(desc, device, reflection, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        if (!reflection.pushConstants.empty()) {
            LOG_E(TAG, "DX12 push constants require an explicit native address and are currently unsupported");
            return false;
        }
        if (!InitParameterTable(desc, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        vs     = desc.vs;
        psOrCs = desc.ps != nullptr ? desc.ps : desc.cs;
        RootSignatureDescriptor                           rsDesc{};
        std::map<uint32_t, RootSignatureDescriptorSet>    sets;
        std::map<uint32_t, std::pair<uint32_t, uint32_t>> offsets;
        for (const auto &res : reflection.resources) {
            ShaderNativeCategory expected = ShaderNativeCategory::UNKNOWN;
            switch (res.type) {
            case ShaderResourceType::UNIFORM_BUFFER:
            case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC: expected = ShaderNativeCategory::CBV; break;
            case ShaderResourceType::SAMPLED_IMAGE: expected = ShaderNativeCategory::SRV; break;
            case ShaderResourceType::STORAGE_IMAGE: expected = ShaderNativeCategory::UAV; break;
            case ShaderResourceType::SAMPLER: expected = ShaderNativeCategory::SAMPLER; break;
            case ShaderResourceType::STORAGE_BUFFER:
            case ShaderResourceType::STORAGE_BUFFER_DYNAMIC:
                if (res.access == ShaderResourceAccess::READ_ONLY)
                    expected = ShaderNativeCategory::SRV;
                else if (res.access == ShaderResourceAccess::READ_WRITE)
                    expected = ShaderNativeCategory::UAV;
                break;
            default: break;
            }
            if (expected == ShaderNativeCategory::UNKNOWN || !res.count) {
                LOG_E(TAG, "Unsupported DX12 resource metadata");
                return false;
            }
            const bool dynamic = res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
            if (dynamic && (res.descriptorArray || res.count != 1)) {
                LOG_E(TAG, "DX12 root descriptors cannot represent descriptor arrays");
                return false;
            }
            auto      &offset           = expected == ShaderNativeCategory::SAMPLER ? offsets[res.set].second : offsets[res.set].first;
            const auto descriptorOffset = offset;
            if (!dynamic) {
                if (res.count > UINT32_MAX - offset)
                    return false;
                offset += res.count;
            }
            ShaderStageFlags covered;
            for (const auto &native : reflection.nativeBindings) {
                if (native.set != res.set || native.binding != res.binding)
                    continue;
                if (native.category != expected || native.count != res.count || !res.stages.TestBit(native.stage) || covered.TestBit(native.stage)) {
                    LOG_E(TAG, "Invalid or duplicate DX12 stage binding");
                    return false;
                }
                RootSignatureDescriptorRange range{};
                range.type             = res.type;
                range.binding          = res.binding;
                range.count            = res.count;
                range.visibility       = native.stage;
                range.access           = res.access;
                range.category         = native.category;
                range.nativeRegister   = native.index;
                range.nativeSpace      = native.space;
                range.descriptorOffset = descriptorOffset;
                sets[res.set].ranges.push_back(range);
                covered |= native.stage;
            }
            if (covered != res.stages || !covered.value) {
                LOG_E(TAG, "DX12 resource is missing its exact native stage mapping");
                return false;
            }
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
