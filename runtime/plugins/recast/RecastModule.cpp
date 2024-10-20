//
// Created by blues on 2024/9/1.
//
#include <framework/interface/IModule.h>
#include <framework/world/World.h>
#include <framework/asset/AssetManager.h>
#include <navigation/NaviMeshFactory.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastNaviMeshGenerator.h>

namespace sky::ai {

    class RecastNaviMapFactory : public NaviMeshFactory::Impl {
    public:
        RecastNaviMapFactory() = default;
        ~RecastNaviMapFactory() override = default;

        NaviMesh* CreateNaviMesh() override
        {
            return new RecastNaviMesh();
        }

        NaviMeshGenerator* CreateGenerator() override
        {
            return new RecastNaviMeshGenerator();
        }
    };

    class RecastModule : public IModule {
    public:
        RecastModule() = default;
        ~RecastModule() override = default;

        bool Init(const StartArguments &args) override
        {
            NaviMeshFactory::Get()->Register(new RecastNaviMapFactory());
            return true;
        }

        void Start() override
        {
        }

        void Shutdown() override
        {
        }
    };
} // namespace sky::phy
REGISTER_MODULE(sky::ai::RecastModule)