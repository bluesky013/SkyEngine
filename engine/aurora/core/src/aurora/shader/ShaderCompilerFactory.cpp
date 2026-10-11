//
// ShaderCompilerFactory: process-wide registry for the optional compiler.
//

#include <aurora/shader/IShaderCompiler.h>

namespace sky::aurora {

    ShaderCompilerFactory &ShaderCompilerFactory::Get()
    {
        return *Singleton<ShaderCompilerFactory>::Get();
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
