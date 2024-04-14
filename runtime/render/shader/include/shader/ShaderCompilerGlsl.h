//
// Created by blues on 2024/2/18.
//

#pragma once

#include <shader/ShaderCompiler.h>

namespace sky {

    class ShaderCompilerGlsl : public ShaderCompilerBase {
    public:
        ShaderCompilerGlsl() = default;
        ~ShaderCompilerGlsl() override;

        bool Init();
        bool CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result) override;
    };

} // namespace sky