//
// Created by blues on 2024/2/3.
//

#pragma once

#include <shader/ShaderCompiler.h>

// dxcompiler
#include <windows.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>

namespace sky {

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    class ShaderCompilerDXC : public ShaderCompilerBase {
    public:
        ShaderCompilerDXC() = default;
        ~ShaderCompilerDXC() override = default;

        bool Init();
        bool CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result) override;

    private:
        void BuildReflectionDXIL(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result);

        ComPtr<IDxcUtils> dxcUtils;
        ComPtr<IDxcCompiler3> dxcCompiler;
        ComPtr<IDxcContainerReflection> containerReflection;
    };

} // namespace sky