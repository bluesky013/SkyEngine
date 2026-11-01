//
// Aurora shader compiler module: registers the Slang compiler implementation
// into ShaderCompilerFactory. Optional: builds without this module fall back to
// cache-only shader resolution.
//

#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <framework/interface/IModule.h>

#include <memory>

namespace sky::aurora {

    class AuroraShaderCompilerModule : public IModule {
    public:
        AuroraShaderCompilerModule()           = default;
        ~AuroraShaderCompilerModule() override = default;

        bool Init(const StartArguments &args) override;
        void Shutdown() override;

    private:
        std::shared_ptr<ShaderCompilerSlang> mCompiler;
    };

    bool AuroraShaderCompilerModule::Init(const StartArguments &args)
    {
        if (mCompiler)
            return true;
        mCompiler = std::make_shared<ShaderCompilerSlang>();
        if (ShaderCompilerFactory::Get().RegisterOwned(mCompiler))
            return true;
        mCompiler.reset();
        return false;
    }

    void AuroraShaderCompilerModule::Shutdown()
    {
        ShaderCompilerFactory::Get().Unregister(mCompiler.get());
        mCompiler.reset();
    }

} // namespace sky::aurora
REGISTER_MODULE_TAGGED(sky::aurora::AuroraShaderCompilerModule)
