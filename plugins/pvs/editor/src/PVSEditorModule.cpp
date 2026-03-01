//
// Created by Zach Lee on 2026/2/23.
//

#include <pvs/editor/PVSEditorModule.h>
#include <pvs/editor/PVSVolume.h>
#include <pvs/editor/PVSBakePipeline.h>
#include <pvs/PVSCulling.h>
#include <editor/framework/EditorActorCreation.h>
#include <framework/world/ComponentFactory.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky::editor {

    PVSEditorModule::PVSEditorModule()
    {
        binder.Bind(this);
    }

    bool PVSEditorModule::Init(const StartArguments &args)
    {
        PVSModule::Init(args);

        auto *serializationContext = SerializationContext::Get();
        PVSVolume::Reflect(serializationContext);

        {
            static std::string GROUP = "Volume";
            ComponentFactory::Get()->RegisterComponent<PVSVolume>(GROUP);
        }

        volumeCreator = std::make_unique<PVSVolumeCreator>();
        EditorActorCreation::Get()->RegisterCreation(volumeCreator.get());

        Renderer::Get()->RegisterRenderFeature<PVSDrawFeatureProcessor>();
        return true;
    }

    void PVSEditorModule::Shutdown()
    {
        EditorActorCreation::Get()->UnRegisterCreation(volumeCreator.get());
        volumeCreator = nullptr;
        binder.Reset();

        PVSModule::Shutdown();
    }

    void PVSEditorModule::OnCreateRenderScene(RenderScene* scene)
    {
        PVSModule::OnCreateRenderScene(scene);

        const auto& persistID = scene->GetPersistentID();
        if (persistID) {
            FilePath basePath = FilePath("Baked/PVS/" + persistID.ToString());
            pvsCulling->Init(basePath);
        }

        auto boxTech = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/box.tech");
        boxTech->BlockUntilLoaded();
        pvsCulling->SetVisualizer(new PVSVisualizer(scene, CreateGfxTechFromAsset(boxTech)));
    }

    void PVSEditorModule::Gather(std::list<CounterPtr<IWorldBuilder>> &builders) const
    {
        builders.emplace_back(new PVSWorldBuilder());
    }

} // namespace sky::editor