//
// Navigation builder module: registers the nav mesh asset builder with the framework
// offline builder. Owned by the navigation subsystem, not by any renderer.
//

#include <builder/navigation/NaviMeshBuilder.h>
#include <navigation/NaviMeshSource.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::builder {

    class NaviMeshBuilderModule : public IModule {
    public:
        NaviMeshBuilderModule()           = default;
        ~NaviMeshBuilderModule() override = default;

        bool Init(const StartArguments &args) override
        {
            ai::NaviMeshSourceData::Reflect(SerializationContext::Get());
            AssetBuilderManager::Get()->RegisterBuilder(new NaviMeshBuilder());
            return true;
        }

        void Tick(float delta) override {}
    };

} // namespace sky::builder
REGISTER_MODULE(sky::builder::NaviMeshBuilderModule)
