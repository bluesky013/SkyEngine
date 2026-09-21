//
// Aurora cook module: registers the aurora asset builders with the framework
// offline builder (mirrors the legacy SkyRender.Builder module).
//

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/cook/image/AuroraImageBuilder.h>
#include <builder/audio/AudioBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora {

    class AuroraCookModule : public IModule {
    public:
        AuroraCookModule()           = default;
        ~AuroraCookModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override {}
    };

    bool AuroraCookModule::Init(const StartArguments &args)
    {
        // Idempotent: guarantees the aurora asset handlers exist for SaveAsset.
        AuroraReflection(SerializationContext::Get());

        AssetBuilderManager::Get()->RegisterBuilder(new AuroraImageBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new builder::AudioBuilder());
        return true;
    }

} // namespace sky::aurora
REGISTER_MODULE(sky::aurora::AuroraCookModule)
