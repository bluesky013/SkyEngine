//
// Created by blues on 2024/2/3.
//

#include <shader/ShaderCompilerDXC.h>
#include <core/logger/Logger.h>

#include <vector>

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
            LOG_E(TAG, "Compile info %s\n", (char*)pErrors->GetBufferPointer());
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
        return true;
    }

} // namespace sky