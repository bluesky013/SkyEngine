//
// Terrain feature plugin module: registers the terrain component and asset handling.
//

#include <terrain/TerrainAsset.h>
#include <terrain/components/TerrainComponent.h>

#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::terrain {

    class TerrainModule : public IModule {
    public:
        TerrainModule()           = default;
        ~TerrainModule() override = default;

        bool Init(const StartArguments &args) override { return true; }

        void Start() override
        {
            auto *context = SerializationContext::Get();
            TerrainComponent::Reflect(context);
            RegisterTerrainAssetType();
        }

        void Shutdown() override {}
    };

} // namespace sky::terrain
REGISTER_MODULE(sky::terrain::TerrainModule)
