//
// Created on 2026/10/30.
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

        void RegisterMeta() override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new NaviMeshBuilder());
        }

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Tick(float delta) override
        {
        }
    };

} // namespace sky::builder
REGISTER_MODULE_TAGGED(sky::builder::NaviMeshBuilderModule)
