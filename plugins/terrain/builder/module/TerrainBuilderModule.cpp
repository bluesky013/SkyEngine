//
// Terrain builder module: registers the terrain asset builder with the framework offline builder.
// Owned by the terrain subsystem, not by any renderer.
//

#include <builder/terrain/TerrainBuilder.h>
#include <terrain/TerrainAsset.h>
#include <terrain/TerrainSource.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::builder {

    class TerrainBuilderModule : public IModule {
    public:
        TerrainBuilderModule()           = default;
        ~TerrainBuilderModule() override = default;

        bool Init(const StartArguments &args) override
        {
            terrain::TerrainSourceData::Reflect(SerializationContext::Get());
            AssetBuilderManager::Get()->RegisterBuilder(new TerrainBuilder());
            return true;
        }

        void Tick(float delta) override {}
    };

} // namespace sky::builder
REGISTER_MODULE(sky::builder::TerrainBuilderModule)
