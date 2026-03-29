//
// Created by blues on 2025/5/18.
//

#include <render/editor/RenderEditorModule.h>
#include <render/editor/animation/SkeletonPreviewWindow.h>
#include <render/editor/MaterialCreator.h>
#include <render/editor/AnimationCreator.h>

#include <editor/framework/AssetBrowserWidget.h>
#include <editor/framework/AssetCreator.h>

#include <render/adaptor/assets/SkeletonAsset.h>


namespace sky::editor {

    bool RenderEditorModule::Init(const StartArguments &args)
    {
        if (!RenderModule::Init(args)) {
            return false;
        }

        // asset
        AssetCreatorManager::Get()->RegisterTool(Name("Material"), new MaterialInstanceCreator());
        AssetCreatorManager::Get()->RegisterTool(Name("Animation Graph"), new AnimationGraphCreator());

        // create
        RegisterActorCreators<RenderCubeActorCreator>(BuiltinGeometryType::CUBE);

        // preview
        AssetPreviewManager::Get()->Register(AssetTraits<Skeleton>::ASSET_TYPE, new SkeletonPreviewWindow());
        return true;
    }

    void RenderEditorModule::Shutdown()
    {
//        EditorToolManager::Get()->UnRegisterTool(Name("Terrain"));

        RenderModule::Shutdown();
    }
} // namespace sky::editor
