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
#include <memory>
#include <mutex>

namespace sky::aurora {

    class IShaderCompiler {
    public:
        virtual ~IShaderCompiler() = default;

        virtual bool                   Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) = 0;
        virtual bool InspectVariants(const ShaderCompileDesc &desc, std::vector<ShaderVariantBinding> &bindings, std::string &error)
        {
            std::vector<ShaderVariantBinding> candidate;
            if (desc.schema)
                for (const auto &entry : desc.schema->entries) {
                    if (!entry.symbol.Empty()) {
                        error = "Compiler does not support variant declaration inspection";
                        return false;
                    }
                    candidate.push_back({std::string(entry.key.GetStr()), {}, entry.type, ShaderVariantLowering::COMPILE, 0, entry.defaultValue});
                }
            bindings = std::move(candidate);
            return true;
        }
        virtual ShaderProducerIdentity GetProducerIdentity() const                                         = 0;
        virtual ShaderProducerIdentity GetProducerIdentity(ShaderTarget) const
        {
            return GetProducerIdentity();
        }
    };

    struct ShaderCompilerLease {
        // Release the compiler and its deleter before unloading its module code.
        std::shared_ptr<const void>      module;
        std::shared_ptr<IShaderCompiler> compiler;
    };
    // Process-wide (cross-DLL) registry: Aurora.Core is a static lib linked into several module DLLs,
    // so the instance lives in the shared Environment (Singleton) rather than a function-local static.
    class ShaderCompilerFactory : public Singleton<ShaderCompilerFactory> {
    public:
        static ShaderCompilerFactory &Get();

        void Register(IShaderCompiler *compiler);
        bool RegisterOwned(std::shared_ptr<IShaderCompiler> compiler, std::shared_ptr<const void> module = {});
        void Unregister(IShaderCompiler *compiler);
        // Borrowed registrations are synchronous-only and remain caller-owned.
        std::shared_ptr<const ShaderCompilerLease> Acquire(bool allowBorrowed = false) const;
        IShaderCompiler                           *GetCompiler() const;

    private:
        friend class Singleton<ShaderCompilerFactory>;

        ShaderCompilerFactory() = default;

        IShaderCompiler                 *mCompiler = nullptr;
        std::shared_ptr<const void>      mModule;
        std::shared_ptr<IShaderCompiler> mOwnedCompiler;
        mutable std::mutex               mMutex;
    };

} // namespace sky::aurora
