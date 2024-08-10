//
// Created by Zach Lee on 2023/2/20.
//

#include <builder/render/ImageBuilder.h>
#include <builder/render/MaterialBuilder.h>
#include <builder/render/PrefabBuilder.h>
#include <builder/render/ShaderBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <builder/render/VertexLibraryBuilder.h>
#include <builder/render/MeshBuilder.h>
#include <builder/render/AnimationBuilder.h>
#include <builder/render/SkeletonBuilder.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <shader/ShaderCompiler.h>

namespace sky::builder {

    class BuilderModule : public IModule {
    public:
        BuilderModule() = default;
        ~BuilderModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override {}
        void ProcessPreloadAssets();

    private:
        NativeFileSystemPtr engineFs;
        NativeFileSystemPtr workSpaceFs;
    };

    bool BuilderModule::Init(const StartArguments &args)
    {
        auto *am = AssetBuilderManager::Get();
        am->RegisterBuilder(new ShaderBuilder());
        am->RegisterBuilder(new TechniqueBuilder());
        am->RegisterBuilder(new MaterialBuilder());
        am->RegisterBuilder(new VertexLibraryBuilder());
        am->RegisterBuilder(new ImageBuilder());
        am->RegisterBuilder(new PrefabBuilder());
        am->RegisterBuilder(new MeshBuilder());
        am->RegisterBuilder(new AnimationBuilder());
        am->RegisterBuilder(new SkeletonBuilder());

        // init shader compiler
        engineFs = AssetBuilderManager::Get()->GetEngineFs();
        workSpaceFs = AssetBuilderManager::Get()->GetWorkSpaceFs();

        auto *compiler = ShaderCompiler::Get();
        compiler->AddSearchPath(engineFs->GetPath());
        compiler->AddSearchPath(workSpaceFs->GetPath());

        ProcessPreloadAssets();
        return true;
    }

    void BuilderModule::ProcessPreloadAssets()
    {
        auto file = workSpaceFs->OpenFile("configs/render_preload_assets.json");
        if (!file) {
            return;
        }

        auto archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);

        uint32_t count = json.StartArray("assets");

        for (uint32_t i = 0; i < count; ++i) {
            AssetSourcePath path = {};
            path.path = json.LoadString();
            path.bundle = SourceAssetBundle::ENGINE;

            auto source = AssetDataBase::Get()->RegisterAsset(path);
            SKY_ASSERT(source);
            json.NextArrayElement();
        }

        json.End();
    }
}
REGISTER_MODULE(sky::builder::BuilderModule)
