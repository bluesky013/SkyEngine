//
// Created by blues on 2024/2/3.
//

#include <shader/ShaderCompilerDXC.h>
#include <core/logger/Logger.h>

#include <vector>
#include <ranges>
#include <d3d12shader.h>

#include <spirv_cross/spirv_glsl.hpp>

static const char* TAG = "ShaderCompilerDXC";

namespace sky {

    bool ShaderCompilerDXC::Init()
    {
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf()));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler.GetAddressOf()));
        DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(containerReflection.GetAddressOf()));

        return (dxcUtils != nullptr) && (dxcCompiler != nullptr) && (containerReflection != nullptr);
    }

    bool ShaderCompilerDXC::CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result)
    {
        ComPtr<IDxcBlobEncoding> pSource;
        dxcUtils->CreateBlob(desc.source.data(), static_cast<uint32_t>(desc.source.size()), CP_UTF8, pSource.GetAddressOf());

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr      = pSource->GetBufferPointer();
        sourceBuffer.Size     = pSource->GetBufferSize();
        sourceBuffer.Encoding = 0;

        std::vector<std::wstring> dxcArgStrings;

        if (op.target == ShaderCompileTarget::SPIRV) {
            dxcArgStrings.emplace_back(L"-spirv");
            dxcArgStrings.emplace_back(L"-fvk-auto-shift-bindings");
        }

        dxcArgStrings.emplace_back(L"-Zpc"); // column major
        dxcArgStrings.emplace_back(L"-Zi"); // debug info
        dxcArgStrings.emplace_back(L"-Od"); // disable optimize

        // entry point
        dxcArgStrings.emplace_back(L"-E");
        dxcArgStrings.emplace_back(desc.entry.begin(), desc.entry.end());

        // stage
        dxcArgStrings.emplace_back(L"-T");
        if (desc.stage == rhi::ShaderStageFlagBit::VS) {
            dxcArgStrings.emplace_back(L"vs_6_0");
        } else if (desc.stage == rhi::ShaderStageFlagBit::FS) {
            dxcArgStrings.emplace_back(L"ps_6_0");
        } else if (desc.stage == rhi::ShaderStageFlagBit::CS) {
            dxcArgStrings.emplace_back(L"cs_6_0");
        }

        std::vector<const wchar_t *> dxcArgs;
        dxcArgs.reserve(dxcArgStrings.size());
        for (const auto &arg : dxcArgStrings) {
            dxcArgs.push_back(arg.c_str());
        }

        ComPtr<IDxcResult> pCompileResult;
        dxcCompiler->Compile(&sourceBuffer, dxcArgs.data(), static_cast<UINT32>(dxcArgs.size()), nullptr,
                             IID_PPV_ARGS(pCompileResult.GetAddressOf()));

        ComPtr<IDxcBlobUtf8> pErrors;
        pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr);
        if ((pErrors != nullptr) && (pErrors->GetStringLength() > 0))
        {
            LOG_I(TAG, "Compile info %s\n", (char*)pErrors->GetBufferPointer());
        }

        HRESULT status;
        pCompileResult->GetStatus(&status);
        if (FAILED(status)) {
            LOG_E(TAG, "Shader Compile Failed.");
            return false;
        }

        ComPtr<IDxcBlob> program;
        pCompileResult->GetResult(&program);
        if (program != nullptr) {
            result.data.resize(program->GetBufferSize() / sizeof(uint32_t));
            memcpy(result.data.data(), program->GetBufferPointer(), program->GetBufferSize());
        }

        if (op.target == ShaderCompileTarget::SPIRV) {
            BuildReflectionSPIRV(desc.stage, result);
        } else {
            BuildReflectionDXIL(desc.stage, result);
        }

        return true;
    }

    void ShaderCompilerDXC::BuildReflectionDXIL(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result)
    {
        const DxcBuffer reflectionBuffer {
            .Ptr      = result.data.data(),
            .Size     = result.data.size() * sizeof(uint32_t),
            .Encoding = 0,
        };

        // build reflection
        ComPtr<ID3D12ShaderReflection> shaderReflection;
        dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));

        D3D12_SHADER_DESC shaderDesc{};
        shaderReflection->GetDesc(&shaderDesc);

        for (auto i : std::views::iota(0U, shaderDesc.BoundResources)) {
            D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
            shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc);

            result.reflection.resources.emplace_back();
            auto &res = result.reflection.resources.back();
            res.set = shaderInputBindDesc.Space;
            res.binding = shaderInputBindDesc.BindPoint;
            res.count = shaderInputBindDesc.BindCount;
            res.name = shaderInputBindDesc.Name;
            res.visibility = stage;

            if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER) {
                ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
                D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
                shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

                res.type = rhi::DescriptorType::UNIFORM_BUFFER;
                res.size = constantBufferDesc.Size;
                auto iter = std::find_if(result.reflection.types.begin(), result.reflection.types.end(),
                    [&res](const auto &type) {
                    return res.name == type.name;
                });
                if (iter == result.reflection.types.end()) {
                    result.reflection.types.emplace_back();
                    auto &type = result.reflection.types.back();
                    type.name = res.name;
                    auto &variables = type.variables;

                    for (auto j : std::views::iota(0U, constantBufferDesc.Variables)) {
                        ID3D12ShaderReflectionVariable *variable = shaderReflectionConstantBuffer->GetVariableByIndex(j);

                        D3D12_SHADER_VARIABLE_DESC varDesc{};
                        variable->GetDesc(&varDesc);

                        variables.emplace_back();
                        auto &var = variables.back();
                        var.name = varDesc.Name;
                        var.offset = varDesc.StartOffset;
                        var.size = varDesc.Size;
                        var.set     = res.set;
                        var.binding = res.binding;
                    }
                }
            } else if (shaderInputBindDesc.Type == D3D_SIT_SAMPLER) {
                res.type = rhi::DescriptorType::SAMPLER;
            } else if (shaderInputBindDesc.Type == D3D_SIT_TEXTURE) {
                res.type = rhi::DescriptorType::SAMPLED_IMAGE;
            } else if (shaderInputBindDesc.Type == D3D_SIT_UAV_RWTYPED) {
                res.type = rhi::DescriptorType::STORAGE_IMAGE;
            } else if (shaderInputBindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED) {
                res.type = rhi::DescriptorType::STORAGE_BUFFER;
            }
        }
    }

    void ShaderCompilerDXC::BuildReflectionSPIRV(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result)
    {
        spirv_cross::CompilerGLSL compiler(result.data.data(), result.data.size());
        spirv_cross::ShaderResources resources = compiler.get_shader_resources(compiler.get_active_interface_variables());

        auto remap = [&compiler, &result, stage](auto &resources, rhi::DescriptorType type) {
            for (auto &resource : resources) {
                auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

                const auto &resType = compiler.get_type(resource.type_id);

                result.reflection.resources.emplace_back();
                auto &res = result.reflection.resources.back();

                res.type = type;
                res.binding = binding;
                res.set = set;
                res.visibility = stage;
                res.name = compiler.get_name(resource.id);
                res.size = 0;
                res.count = resType.array.empty() ? 0 : resType.array[0];

                if (type == rhi::DescriptorType::UNIFORM_BUFFER) {
                    res.size = static_cast<uint32_t>(compiler.get_declared_struct_size(resType));

                    auto iter = std::find_if(result.reflection.types.begin(), result.reflection.types.end(),
                        [&res](const auto &type) {
                        return res.name == type.name;
                    });

                    if (iter == result.reflection.types.end()) {
                        result.reflection.types.emplace_back();
                        auto &structType = result.reflection.types.back();
                        structType.name = res.name;
                        auto &variables = structType.variables;

                        for (auto i : std::views::iota(0U, resType.member_types.size())) {
                            variables.emplace_back();
                            auto &var   = variables.back();
                            var.set     = set;
                            var.binding = binding;
                            var.size    = static_cast<uint32_t>(compiler.get_declared_struct_member_size(resType, i));
                            var.offset  = compiler.type_struct_member_offset(resType, i);
                            var.name    = compiler.get_member_name(resource.base_type_id, i);
                        }
                    }
                }
            }
        };

        remap(resources.uniform_buffers, rhi::DescriptorType::UNIFORM_BUFFER);
        remap(resources.storage_buffers, rhi::DescriptorType::STORAGE_BUFFER);
        remap(resources.separate_images, rhi::DescriptorType::SAMPLED_IMAGE);
        remap(resources.separate_samplers, rhi::DescriptorType::SAMPLER);
        remap(resources.storage_images, rhi::DescriptorType::STORAGE_IMAGE);
        remap(resources.subpass_inputs, rhi::DescriptorType::INPUT_ATTACHMENT);
    }

} // namespace sky