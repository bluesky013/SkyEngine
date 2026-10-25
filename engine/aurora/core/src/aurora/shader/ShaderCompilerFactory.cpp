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
        mModule.reset();
        mCompiler = compiler;
    }

    IShaderCompiler *ShaderCompilerFactory::GetCompiler() const
    {
        std::lock_guard lock(mMutex);
        return mCompiler;
    }

    bool ShaderCompilerFactory::RegisterOwned(std::shared_ptr<IShaderCompiler> compiler, std::shared_ptr<const void> module)
    {
        std::lock_guard lock(mMutex);
        if (!compiler || mCompiler)
            return false;
        mCompiler      = compiler.get();
        mModule        = std::move(module);
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
        mModule.reset();
    }
    std::shared_ptr<const ShaderCompilerLease> ShaderCompilerFactory::Acquire(bool allowBorrowed) const
    {
        std::lock_guard lock(mMutex);
        // Allocate the lease in the consumer; a module-owned control block cannot safely unload itself.
        if (mOwnedCompiler)
            return std::make_shared<const ShaderCompilerLease>(mModule, mOwnedCompiler);
        if (!allowBorrowed || !mCompiler)
            return {};
        return std::make_shared<const ShaderCompilerLease>(nullptr, std::shared_ptr<IShaderCompiler>(mCompiler, [](IShaderCompiler *) {}));
    }

} // namespace sky::aurora
