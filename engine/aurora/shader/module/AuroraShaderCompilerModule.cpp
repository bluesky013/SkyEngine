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
        std::unique_ptr<ShaderCompilerSlang> mCompiler;
    };

    bool AuroraShaderCompilerModule::Init(const StartArguments &args)
    {
        mCompiler = std::make_unique<ShaderCompilerSlang>();
        ShaderCompilerFactory::Get().Register(mCompiler.get());
        return true;
    }

    void AuroraShaderCompilerModule::Shutdown()
    {
        ShaderCompilerFactory::Get().Register(nullptr);
        mCompiler.reset();
    }

} // namespace sky::aurora
REGISTER_MODULE(sky::aurora::AuroraShaderCompilerModule)
