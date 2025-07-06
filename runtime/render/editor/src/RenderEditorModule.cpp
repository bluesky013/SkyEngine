//
// Created by blues on 2025/5/18.
//

#include <render/editor/RenderEditorModule.h>
#include <render/editor/animation/SkeletonPreviewWindow.h>
#include <editor/framework/AssetBrowserWidget.h>
#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky::editor {

    bool RenderEditorModule::Init(const StartArguments &args)
    {
        if (!RenderModule::Init(args)) {
            return false;
        }
        RegisterActorCreators<RenderCubeActorCreator>(BuiltinGeometryType::CUBE);
        AssetPreviewManager::Get()->Register(AssetTraits<Skeleton>::ASSET_TYPE, new SkeletonPreviewWindow());
        return true;
    }

    void RenderEditorModule::Shutdown()
    {
//        EditorToolManager::Get()->UnRegisterTool(Name("Terrain"));

        RenderModule::Shutdown();
    }
} // namespace sky::editor
