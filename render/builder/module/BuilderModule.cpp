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
#include <shader/ShaderCompiler.h>

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

        auto *am = AssetManager::Get();
        am->RegisterBuilder(new ShaderBuilder());
        am->RegisterBuilder(new TechniqueBuilder());
        am->RegisterBuilder(new MaterialBuilder());
        am->RegisterBuilder(new VertexLibraryBuilder());
        am->RegisterBuilder(new ImageBuilder());
        am->RegisterBuilder(new PrefabBuilder());


        // init shader compiler
        auto *compiler = sl::ShaderCompiler::Get();
        for (const auto &path : am->GetSearchPathList()) {
            compiler->AddSearchPath(path.path);
        }
        return true;
    }
}
REGISTER_MODULE(sky::builder::BuilderModule)
