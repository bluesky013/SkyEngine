//
// Created by blues on 2024/2/3.
//

#if _WIN32

#include <shader/ShaderCompilerDXC.h>
#include <shader/ShaderCross.h>
#include <core/logger/Logger.h>

#include <vector>
#include <ranges>
#include <d3d12shader.h>
#include <spirv-tools/libspirv.hpp>

static const char* TAG = "ShaderCompilerDXC";

namespace sky {

    void BuildReflectionVertexInput(const D3D12_SHADER_DESC& shaderDesc, ID3D12ShaderReflection* shaderReflection, ShaderBuildResult &result)
    {
        std::vector<VertexStageAttribute> &attributes = result.reflection.attributes;
        for (auto i : std::views::iota(0U, shaderDesc.InputParameters)) {
            D3D12_SIGNATURE_PARAMETER_DESC sigDesc = {};
            shaderReflection->GetInputParameterDesc(i, &sigDesc);

            attributes.emplace_back(VertexStageAttribute{
                sigDesc.SemanticName,
                sigDesc.SemanticIndex,
            });
        }
    }

    void BuildReflectionShaderResourceDXIL(const D3D12_SHADER_DESC& shaderDesc, ID3D12ShaderReflection* shaderReflection,
                             rhi::ShaderStageFlagBit stage, ShaderBuildResult &result)
    {
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

    void BuildReflectionDXIL(IDxcUtils* dxcUtils, rhi::ShaderStageFlagBit stage, ShaderBuildResult &result)
    {
        const DxcBuffer reflectionBuffer {
            .Ptr      = result.data.data(),
            .Size     = result.data.size() * sizeof(uint32_t),
            .Encoding = DXC_CP_ACP,
        };

        ComPtr<ID3D12ShaderReflection> shaderReflection;
        dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));

        D3D12_SHADER_DESC shaderDesc{};
        shaderReflection->GetDesc(&shaderDesc);
        BuildReflectionVertexInput(shaderDesc, shaderReflection.Get(), result);
        BuildReflectionShaderResourceDXIL(shaderDesc, shaderReflection.Get(), stage, result);
    }

    ShaderCompilerDXC::~ShaderCompilerDXC()
    {
        dxcUtils = nullptr;
        dxcCompiler = nullptr;
        containerReflection = nullptr;
        dxcModule = nullptr;
    }

    bool ShaderCompilerDXC::Init()
    {
        dxcModule = std::make_unique<DynamicModule>("dxcompiler");
        if (!dxcModule->Load()) {
            return false;
        }
        createInstanceProc = reinterpret_cast<DxcCreateInstanceProc>(dxcModule->GetAddress("DxcCreateInstance"));
        SKY_ASSERT(createInstanceProc != nullptr)

        createInstanceProc(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf()));
        createInstanceProc(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler.GetAddressOf()));
        createInstanceProc(CLSID_DxcContainerReflection, IID_PPV_ARGS(containerReflection.GetAddressOf()));

        return (dxcUtils != nullptr) && (dxcCompiler != nullptr) && (containerReflection != nullptr);
    }

    std::string ShaderCompilerDXC::Disassemble(const std::vector<uint32_t>& binary, ShaderCompileTarget target) const
    {
        std::string text;

        spvtools::SpirvTools tool(spv_target_env::SPV_ENV_VULKAN_1_3);
        tool.Disassemble(binary, &text, SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET | SPV_BINARY_TO_TEXT_OPTION_INDENT);

        return text;
    }

    bool ShaderCompilerDXC::CheckOption(const sky::ShaderCompileOption &op) const
    {
        if (op.target == ShaderCompileTarget::SPIRV) {
            return op.useMeshShader;
        }
        return true;
    }

    bool ShaderCompilerDXC::CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result)
    {
        std::map<std::string, uint8_t> preDefined;

        std::stringstream ss;
        if (op.option) {
            for (auto &val: op.option->values) {
                ss << "#define " << val.first << " " << static_cast<uint32_t>(val.second) << "\n";
            }

            for (auto &val : preDefined) {
                ss << "#define " << val.first << " " << static_cast<uint32_t>(val.second) << "\n";
            }
        }
        std::string finalSource = ss.str() + desc.source;

        ComPtr<IDxcBlobEncoding> pSource;
        dxcUtils->CreateBlob(finalSource.data(), static_cast<uint32_t>(finalSource.size()), CP_UTF8, pSource.GetAddressOf());

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr      = pSource->GetBufferPointer();
        sourceBuffer.Size     = pSource->GetBufferSize();
        sourceBuffer.Encoding = 0;

        std::vector<std::wstring> dxcArgStrings;

        if (op.target == ShaderCompileTarget::SPIRV) {
            dxcArgStrings.emplace_back(L"-spirv");
            dxcArgStrings.emplace_back(L"-fvk-use-gl-layout");
            dxcArgStrings.emplace_back(L"-fspv-target-env=vulkan1.3");
            dxcArgStrings.emplace_back(L"-fspv-extension=SPV_EXT_mesh_shader");
            //            dxcArgStrings.emplace_back(L"-fvk-auto-shift-bindings");
            //            dxcArgStrings.emplace_back(L"-Zpr");
        } else {
            //            dxcArgStrings.emplace_back(L"-Zpc");
        }


        //        dxcArgStrings.emplace_back(L"-Zi"); // debug info
        dxcArgStrings.emplace_back(L"-Od"); // disable optimize

        // entry point
        dxcArgStrings.emplace_back(L"-E");
        dxcArgStrings.emplace_back(desc.entry.begin(), desc.entry.end());

        // stage
        dxcArgStrings.emplace_back(L"-T");
        if (desc.stage == rhi::ShaderStageFlagBit::VS) {
            dxcArgStrings.emplace_back(L"vs_6_1");
        } else if (desc.stage == rhi::ShaderStageFlagBit::FS) {
            dxcArgStrings.emplace_back(L"ps_6_1");
        } else if (desc.stage == rhi::ShaderStageFlagBit::CS) {
            dxcArgStrings.emplace_back(L"cs_6_1");
        } else if (desc.stage == rhi::ShaderStageFlagBit::TAS) {
            dxcArgStrings.emplace_back(L"as_6_5");
        } else if (desc.stage == rhi::ShaderStageFlagBit::MS) {
            dxcArgStrings.emplace_back(L"ms_6_5");
        }

        std::vector<const wchar_t *> dxcArgs;
        dxcArgs.reserve(dxcArgStrings.size());
        for (const auto &arg : dxcArgStrings) {
            dxcArgs.push_back(arg.c_str());
        }

        ComPtr<IDxcResult> pCompileResult;
        dxcCompiler->Compile(&sourceBuffer, dxcArgs.data(), static_cast<UINT32>(dxcArgs.size()), nullptr,
                             IID_PPV_ARGS(pCompileResult.GetAddressOf()));

        HRESULT status;
        pCompileResult->GetStatus(&status);
        if (FAILED(status)) {
            ComPtr<IDxcBlobUtf8> pErrors;
            pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr);
            if ((pErrors != nullptr) && (pErrors->GetStringLength() > 0)) {
                auto *errorInfo = static_cast<char*>(pErrors->GetBufferPointer());
                LOG_E(TAG, "Compile info %s\n", errorInfo);
            }
            return false;
        }

        ComPtr<IDxcBlob> program;
        pCompileResult->GetResult(&program);
        if (program != nullptr) {
            result.data.resize(program->GetBufferSize() / sizeof(uint32_t));
            memcpy(result.data.data(), program->GetBufferPointer(), program->GetBufferSize());
        }

        auto str = Disassemble(result.data, op.target);
        if (op.target == ShaderCompileTarget::SPIRV) {
            BuildReflectionSPIRV(desc.stage, result);
        } else {
            BuildReflectionDXIL(dxcUtils.Get(), desc.stage, result);
        }

        return true;
    }
} // namespace sky

#endif