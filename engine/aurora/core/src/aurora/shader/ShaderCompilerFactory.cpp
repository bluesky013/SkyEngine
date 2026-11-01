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
        std::lock_guard lock(mMutex);
        mOwnedCompiler.reset();
        mCompiler = compiler;
    }

    IShaderCompiler *ShaderCompilerFactory::GetCompiler() const
    {
        std::lock_guard lock(mMutex);
        return mCompiler;
    }

    bool ShaderCompilerFactory::RegisterOwned(std::shared_ptr<IShaderCompiler> compiler)
    {
        std::lock_guard lock(mMutex);
        if (!compiler || mCompiler)
            return false;
        mCompiler      = compiler.get();
        mOwnedCompiler = std::move(compiler);
        return true;
    }
    void ShaderCompilerFactory::Unregister(IShaderCompiler *compiler)
    {
        std::lock_guard lock(mMutex);
        if (mCompiler != compiler)
            return;
        mCompiler = nullptr;
        mOwnedCompiler.reset();
    }
    std::shared_ptr<const ShaderCompilerLease> ShaderCompilerFactory::Acquire(bool allowBorrowed) const
    {
        std::lock_guard lock(mMutex);
        if (mOwnedCompiler)
            return std::make_shared<const ShaderCompilerLease>(mOwnedCompiler);
        if (!allowBorrowed || !mCompiler)
            return {};
        return std::make_shared<const ShaderCompilerLease>(std::shared_ptr<IShaderCompiler>(mCompiler, [](IShaderCompiler *) {}));
    }

} // namespace sky::aurora
