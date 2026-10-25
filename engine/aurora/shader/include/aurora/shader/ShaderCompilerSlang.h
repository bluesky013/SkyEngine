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

        bool                   Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) override;
        bool InspectVariants(const ShaderCompileDesc &desc, std::vector<ShaderVariantBinding> &bindings, std::string &error) override;
        ShaderProducerIdentity GetProducerIdentity() const override;
        ShaderProducerIdentity GetProducerIdentity(ShaderTarget target) const override;

        // Inspect module-level uniform layouts without requiring an entry point.
        bool ReflectBlocks(const std::string &source, ShaderTarget target, ShaderReflection &reflection, std::string &error);

    private:
        bool CompileImpl(const ShaderCompileDesc &desc, ShaderCompileResult &result,
                         const std::vector<ShaderVariantBinding> *bindings, bool inspect);
        static bool InitGlobalSession();
    };

} // namespace sky::aurora
