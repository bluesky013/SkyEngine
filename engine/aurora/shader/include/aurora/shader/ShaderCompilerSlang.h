//
// Aurora shader compiler: Slang backend (SPIRV / MSL / DXIL direct emission).
// Implements the abstract IShaderCompiler from aurora/core; registered into
// ShaderCompilerFactory by the AuroraShaderCompiler module.
//

#pragma once

#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderVariant.h>

#include <string>

namespace sky::aurora {

    class ShaderCompilerSlang : public IShaderCompiler {
    public:
        ShaderCompilerSlang()           = default;
        ~ShaderCompilerSlang() override = default;

        ShaderCompilerSlang(const ShaderCompilerSlang &)            = delete;
        ShaderCompilerSlang &operator=(const ShaderCompilerSlang &) = delete;

        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) override;

        // Reflect the global-scope blocks (ParameterBlock / cbuffer) of a module.
        // No entry point is required; used by the offline shader header codegen.
        bool ReflectBlocks(const std::string &source, ShaderTarget target, ShaderReflection &reflection, std::string &error);

    private:
        bool InitGlobalSession();
    };

} // namespace sky::aurora
