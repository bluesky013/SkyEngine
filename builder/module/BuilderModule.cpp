//
// Created by Zach Lee on 2023/2/20.
//


#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <builder/render/ShaderBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <builder/prefab/PrefabBuilder.h>

namespace sky::builder {

    class BuilderModule : public IModule {
    public:
        BuilderModule() = default;
        ~BuilderModule() override = default;

        bool Init() override;
        void Start() override {}
        void Stop() override {}
        void Tick(float delta) override {}

    private:
        std::unique_ptr<ShaderBuilder> shaderBuilder;
        std::unique_ptr<PrefabBuilder> prefabBuilder;
        std::unique_ptr<TechniqueBuilder> techBuilder;
    };

    bool BuilderModule::Init()
    {
        shaderBuilder = std::make_unique<ShaderBuilder>();
        techBuilder = std::make_unique<TechniqueBuilder>();
        prefabBuilder = std::make_unique<PrefabBuilder>();

        AssetManager::Get()->RegisterBuilder(".vert", shaderBuilder.get());
        AssetManager::Get()->RegisterBuilder(".frag", shaderBuilder.get());
        AssetManager::Get()->RegisterBuilder(".comp", shaderBuilder.get());

        AssetManager::Get()->RegisterBuilder(".tech", techBuilder.get());

        AssetManager::Get()->RegisterBuilder(".gltf", prefabBuilder.get());
        AssetManager::Get()->RegisterBuilder(".fbx", prefabBuilder.get());


        return true;
    }
}
REGISTER_MODULE(sky::builder::BuilderModule)