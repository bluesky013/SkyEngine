//
// Created by Zach Lee on 2023/2/20.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/MaterialBuilder.h>
#include <builder/render/PrefabBuilder.h>
#include <builder/render/ShaderBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <builder/render/VertexLibraryBuilder.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/Reflection.h>

namespace sky::builder {

    class BuilderModule : public IModule {
    public:
        BuilderModule() = default;
        ~BuilderModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override {}
    };

    bool BuilderModule::Init(const StartArguments &args)
    {
        auto *serializationContext = SerializationContext::Get();
        ReflectRenderAsset(serializationContext);
        ReflectRHI(serializationContext);

//        techBuilder = std::make_unique<TechniqueBuilder>();
//        materialBuilder = std::make_unique<MaterialBuilder>();
//        imageBuilder = std::make_unique<ImageBuilder>();
//        prefabBuilder = std::make_unique<PrefabBuilder>();
//        vtxLibBuilder = std::make_unique<VertexLibraryBuilder>();

        auto *am = AssetManager::Get();
        am->RegisterBuilder(new ShaderBuilder());
        am->RegisterBuilder(new TechniqueBuilder());

//        AssetManager::Get()->RegisterBuilder(".jpg", imageBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".jpeg", imageBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".png", imageBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".dds", imageBuilder.get());
//
//
//        AssetManager::Get()->RegisterBuilder(".tech", techBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".mat", materialBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".mati", materialBuilder.get());
//
//        AssetManager::Get()->RegisterBuilder(".gltf", prefabBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".glb", prefabBuilder.get());
//        AssetManager::Get()->RegisterBuilder(".fbx", prefabBuilder.get());
//
//        AssetManager::Get()->RegisterBuilder(".vtxlib", vtxLibBuilder.get());
        return true;
    }
}
REGISTER_MODULE(sky::builder::BuilderModule)
