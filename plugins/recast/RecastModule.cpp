//
// Created by blues on 2024/9/1.
//
#include <framework/interface/IModule.h>
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
            return true;
        }

        void Start() override
        {
            NaviMeshFactory::Get()->Register(new RecastNaviMapFactory());
        }

        void Shutdown() override
        {
            NaviMeshFactory::Get()->UnRegister();
        }
    };
} // namespace sky::ai
REGISTER_MODULE(sky::ai::RecastModule)