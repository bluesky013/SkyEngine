//
// Created by blues on 2024/11/28.
//

#include <terrain/editor/TerrainEditorModule.h>
#include <terrain/editor/TerrainEditorTool.h>
#include <editor/framework/EditorToolBase.h>

namespace sky::editor {

    bool TerrainEditorModule::Init(const StartArguments &args)
    {
        if (!TerrainModule::Init(args)) {
            return false;
        }
        EditorToolManager::Get()->RegisterTool(Name("Terrain"), new TerrainEditorTool());
        return true;
    }

    void TerrainEditorModule::Shutdown()
    {
        EditorToolManager::Get()->UnRegisterTool(Name("Terrain"));

        TerrainModule::Shutdown();
    }
} // namespace sky::editor