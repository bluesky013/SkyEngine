//
// Aurora cook module: registers the aurora asset builders with the framework
// offline builder (replaces the former SkyRender.Builder module).
//

#include <aurora/cook/image/AuroraImageBuilder.h>
#include <aurora/cook/image/ImageCookSettings.h>
#include <aurora/cook/lodgroup/AuroraLodGroupBuilder.h>
#include <aurora/cook/material/AuroraMaterialBuilder.h>
#include <aurora/cook/material/AuroraMaterialInstanceBuilder.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <aurora/cook/mesh/MeshAuthoringCompiler.h>
#include <aurora/cook/skeleton/AuroraSkeletonBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>

namespace sky::aurora {

    class AuroraCookModule : public IModule {
    public:
        AuroraCookModule()           = default;
        ~AuroraCookModule() override = default;

        void RegisterMeta() override;
        bool Init(const StartArguments &args) override;
        void Shutdown() override;
        void Tick(float delta) override
        {
        }

    private:
        std::shared_ptr<phy::IMeshAuthoringCompiler> meshCompiler;
    };

    void AuroraCookModule::RegisterMeta()
    {
        cook::RegisterImageCookSettings();

        AssetBuilderManager::Get()->RegisterBuilder(new AuroraImageBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMeshBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMaterialBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraMaterialInstanceBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraLodGroupBuilder());
        AssetBuilderManager::Get()->RegisterBuilder(new AuroraSkeletonBuilder());
    }

    bool AuroraCookModule::Init(const StartArguments &args)
    {
        meshCompiler = cook::CreateMeshAuthoringCompiler();
        if (!phy::MeshAuthoringCompilerRegistry::Get()->Register(meshCompiler)) {
            meshCompiler.reset();
            return false;
        }
        return true;
    }

    void AuroraCookModule::Shutdown()
    {
        if (meshCompiler) {
            phy::MeshAuthoringCompilerRegistry::Get()->Unregister(meshCompiler);
            meshCompiler.reset();
        }
    }

} // namespace sky::aurora
REGISTER_MODULE(sky::aurora::AuroraCookModule)
