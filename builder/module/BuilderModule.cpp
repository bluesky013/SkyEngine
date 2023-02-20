//
// Created by Zach Lee on 2023/2/20.
//


#include <framework/interface/IModule.h>
#include <framework/asset/AssetManager.h>
#include <builder/shader/ShaderCompiler.h>
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
        std::unique_ptr<ShaderCompiler> shaderCompiler;
        std::unique_ptr<PrefabBuilder> prefabBuilder;
    };

    bool BuilderModule::Init()
    {
        shaderCompiler = std::make_unique<ShaderCompiler>();
        prefabBuilder = std::make_unique<PrefabBuilder>();

        AssetManager::Get()->RegisterBuilder(".vert", shaderCompiler.get());
        AssetManager::Get()->RegisterBuilder(".frag", shaderCompiler.get());
        AssetManager::Get()->RegisterBuilder(".comp", shaderCompiler.get());
        AssetManager::Get()->RegisterBuilder(".gltf", prefabBuilder.get());
        AssetManager::Get()->RegisterBuilder(".fbx", prefabBuilder.get());
        return true;
    }
}
REGISTER_MODULE(sky::builder::BuilderModule)