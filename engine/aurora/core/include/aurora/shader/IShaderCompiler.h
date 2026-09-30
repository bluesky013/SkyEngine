//
// IShaderCompiler: abstract shader compiler + process-wide registry.
//
// The compiler implementation lives in a separate (optional) module
// (AuroraShaderCompiler) that registers itself here on load. The resolver
// consults GetCompiler(); a null compiler means cache-only resolution.
//

#pragma once

#include <aurora/shader/ShaderCompile.h>

namespace sky::aurora {

    class IShaderCompiler {
    public:
        virtual ~IShaderCompiler() = default;

        virtual bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) = 0;
    };

    class ShaderCompilerFactory {
    public:
        static ShaderCompilerFactory &Get();

        void             Register(IShaderCompiler *compiler);
        IShaderCompiler *GetCompiler() const;

    private:
        IShaderCompiler *mCompiler = nullptr;
    };

} // namespace sky::aurora
