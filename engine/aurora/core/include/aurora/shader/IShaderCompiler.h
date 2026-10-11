//
// IShaderCompiler: abstract shader compiler + process-wide registry.
//
// The compiler implementation lives in a separate (optional) module
// (Aurora.ShaderCompiler) that registers itself here on load. The resolver
// consults GetCompiler(); a null compiler means cache-only resolution.
//

#pragma once

#include <aurora/shader/ShaderCompile.h>

#include <core/environment/Singleton.h>

namespace sky::aurora {

    class IShaderCompiler {
    public:
        virtual ~IShaderCompiler() = default;

        virtual bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) = 0;
    };

    // Process-wide (cross-DLL) registry: Aurora.Core is a static lib linked into several module DLLs,
    // so the instance lives in the shared Environment (Singleton) rather than a function-local static.
    class ShaderCompilerFactory : public Singleton<ShaderCompilerFactory> {
    public:
        static ShaderCompilerFactory &Get();

        void             Register(IShaderCompiler *compiler);
        IShaderCompiler *GetCompiler() const;

    private:
        friend class Singleton<ShaderCompilerFactory>;

        ShaderCompilerFactory() = default;

        IShaderCompiler *mCompiler = nullptr;
    };

} // namespace sky::aurora
