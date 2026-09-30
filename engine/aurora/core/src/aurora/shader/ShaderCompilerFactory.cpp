//
// ShaderCompilerFactory: process-wide registry for the optional compiler.
//

#include <aurora/shader/IShaderCompiler.h>

namespace sky::aurora {

    ShaderCompilerFactory &ShaderCompilerFactory::Get()
    {
        static ShaderCompilerFactory instance;
        return instance;
    }

    void ShaderCompilerFactory::Register(IShaderCompiler *compiler)
    {
        mCompiler = compiler;
    }

    IShaderCompiler *ShaderCompilerFactory::GetCompiler() const
    {
        return mCompiler;
    }

} // namespace sky::aurora
