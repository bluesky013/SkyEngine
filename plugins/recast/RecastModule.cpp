//
// Created on 2026/10/30.
//

#include <core/logger/Logger.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/Any.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NaviMeshSource.h>
#include <navigation/NavigationSystem.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastQueryFilter.h>

namespace sky::ai {
    namespace {
        const NavigationSubSystemConfig *ReadConfig(const Any &value, NavigationSubSystemConfig &defaults)
        {
            return value ? value.GetAsConst<NavigationSubSystemConfig>() : &defaults;
        }
        class RecastNaviMapFactory final : public NaviMeshFactory::Impl {
        public:
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
    } // namespace
    class RecastModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            NaviMeshData::Reflect(SerializationContext::Get());
            NaviMeshSourceData::Reflect(SerializationContext::Get());
            NavigationSubSystemConfig::Reflect(SerializationContext::Get());
            AssetManager::Get()->RegisterAssetHandler<NaviMesh>();
        }
        void Start() override
        {
            factoryRegistered = NaviMeshFactory::Get()->Register(new RecastNaviMapFactory());
            if (!factoryRegistered) {
                LOG_E("Recast", "Navigation backend is already registered");
                return;
            }
            WorldSubSystemRegistration registration;
            registration.factory = [](World &, const Any &value) -> std::unique_ptr<IWorldSubSystem> {
                NavigationSubSystemConfig defaults;
                const auto               *config = ReadConfig(value, defaults);
                if (!config)
                    return nullptr;
                auto        system = std::make_unique<NavigationSystem>();
                std::string error;
                if (!system->Init(*config, error)) {
                    LOG_E("Recast", "%s", error.c_str());
                    return nullptr;
                }
                return system;
            };
            registration.configType        = TypeInfoObj<NavigationSubSystemConfig>::Get()->RtInfo();
            registration.makeDefaultConfig = [] { return Any(std::in_place_type<NavigationSubSystemConfig>); };
            registration.validate          = [](const Any &value, std::string &error) {
                NavigationSubSystemConfig defaults;
                const auto               *config = ReadConfig(value, defaults);
                if (!config) {
                    error = "Unexpected navigation config type";
                    return false;
                }
                NaviAgentConfig    agent;
                NaviMeshResolution resolution;
                uint32_t           capacity = 0;
                return config->MakeSettings(agent, resolution, capacity, error);
            };
            worldRegistered = WorldSubSystemRegistry::Get().Register(Name(INavigationSystem::NAME.data()), std::move(registration));
            if (!worldRegistered) {
                LOG_E("Recast", "Navigation subsystem is already registered");
                NaviMeshFactory::Get()->UnRegister();
                factoryRegistered = false;
            }
        }
        void Shutdown() override
        {
            if (worldRegistered) {
                WorldSubSystemRegistry::Get().Unregister(Name(INavigationSystem::NAME.data()));
                worldRegistered = false;
            }
            if (factoryRegistered) {
                NaviMeshFactory::Get()->UnRegister();
                factoryRegistered = false;
            }
        }

    private:
        bool factoryRegistered = false;
        bool worldRegistered   = false;
    };
} // namespace sky::ai
REGISTER_MODULE_TAGGED(sky::ai::RecastModule)
