//
// Created by blues on 2024/2/3.
//

#pragma once

#if _WIN32

#include <shader/ShaderCompiler.h>

// dxcompiler
#include <windows.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>
#include <core/util/DynamicModule.h>

namespace sky {

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    class ShaderCompilerDXC : public ShaderCompilerBase {
    public:
        ShaderCompilerDXC() = default;
        ~ShaderCompilerDXC() override;

        bool Init();
        bool CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result) override;

    private:
        void BuildReflectionDXIL(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result);

        ComPtr<IDxcUtils> dxcUtils;
        ComPtr<IDxcCompiler3> dxcCompiler;
        ComPtr<IDxcContainerReflection> containerReflection;

        std::unique_ptr<DynamicModule> dxcModule;
        DxcCreateInstanceProc createInstanceProc = nullptr;
    };

} // namespace sky

#endif