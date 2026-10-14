//
// Aurora cook module: registers the aurora asset builders with the framework
// offline builder (mirrors the legacy SkyRender.Builder module).
//

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/cook/image/AuroraImageBuilder.h>
#include <aurora/cook/lodgroup/AuroraLodGroupBuilder.h>
#include <aurora/cook/material/AuroraMaterialBuilder.h>
#include <aurora/cook/material/AuroraMaterialInstanceBuilder.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <aurora/cook/skeleton/AuroraSkeletonBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora {

    class AuroraCookModule : public IModule {
    public:
        AuroraCookModule()           = default;
        ~AuroraCookModule() override = default;

        void RegisterMeta() override;
        bool Init(const StartArguments &args) override;
        void Tick(float delta) override
        {
        }
    };

    void AuroraCookModule::RegisterMeta()
    {
        // Idempotent: guarantees the aurora asset handlers exist for SaveAsset.
        AuroraReflection(SerializationContext::Get());

        AssetBuilderManager::Get()->RegisterBuilder(new AuroraImageBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMeshBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMaterialBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMaterialInstanceBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraLodGroupBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraSkeletonBuilder());
    }

    bool AuroraCookModule::Init(const StartArguments &args)
    {
        return true;
    }

} // namespace sky::aurora
REGISTER_MODULE(sky::aurora::AuroraCookModule)
