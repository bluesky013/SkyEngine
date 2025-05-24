//
// Created by blues on 2025/5/18.
//

#include <render/editor/RenderEditorModule.h>

namespace sky::editor {

    bool RenderEditorModule::Init(const StartArguments &args)
    {
        if (!RenderModule::Init(args)) {
            return false;
        }
        RegisterActorCreators<RenderCubeActorCreator>(BuiltinGeometryType::CUBE);
        return true;
    }

    void RenderEditorModule::Shutdown()
    {
//        EditorToolManager::Get()->UnRegisterTool(Name("Terrain"));

        RenderModule::Shutdown();
    }
} // namespace sky::editor