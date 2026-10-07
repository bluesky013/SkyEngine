//
// Created by blues on 2024/9/1.
//
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationFactory.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NavigationSystem.h>
#include <recast/NavigationSubSystemConfig.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastQueryFilter.h>

#include <memory>
#include <string>

namespace sky::ai {

    class RecastNaviMapFactory : public NaviMeshFactory::Impl {
    public:
        RecastNaviMapFactory()           = default;
        ~RecastNaviMapFactory() override = default;

        NaviMesh *CreateNaviMesh() override
        {
            return new RecastNaviMesh();
        }

        NaviMeshGenerator *CreateGenerator() override
        {
            return new RecastNaviMeshGenerator();
        }

        NaviQueryFilter *CreateQueryFilter() override
        {
            return new RecastQueryFilter();
        }
    };

    class RecastModule : public IModule {
    public:
        RecastModule()           = default;
        ~RecastModule() override = default;

        bool Init(const StartArguments &args) override
        {
            NaviMeshData::Reflect(SerializationContext::Get());
            NavigationSubSystemConfig::Reflect(SerializationContext::Get());
            AssetManager::Get()->RegisterAssetHandler<NaviMesh>();
            return true;
        }

        void Start() override
        {
            NaviMeshFactory::Get()->Register(new RecastNaviMapFactory());

            // Declarative world subsystem: navigation.
            WorldSubSystemRegistry::Get().Register(
                Name(NavigationSystem::NAME.data()),
                WorldSubSystemRegistration{
                    [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return std::make_unique<NavigationSystem>(); },
                    TypeInfoObj<NavigationSubSystemConfig>::Get()->RtInfo(),
                    [] { return Any(std::in_place_type<NavigationSubSystemConfig>); },
                    [](const Any &, std::string &) { return true; },
                });
        }

        void Shutdown() override
        {
            WorldSubSystemRegistry::Get().Unregister(Name(NavigationSystem::NAME.data()));
            NaviMeshFactory::Get()->UnRegister();
        }
    };
} // namespace sky::ai
REGISTER_MODULE(sky::ai::RecastModule)
