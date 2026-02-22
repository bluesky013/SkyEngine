//
// Created by blues on 2024/11/28.
//

#include <terrain/editor/TerrainEditorTool.h>
#include <terrain/editor/TerrainToolWidget.h>

namespace sky::editor {
    bool TerrainEditorTool::Init()
    {
        return true;
    }

    void TerrainEditorTool::Shutdown()
    {

    }

    ToolWidget *TerrainEditorTool::InitToolWidget(QWidget* parent)
    {
        return new TerrainToolWidget(parent);
    }

} // sky::editor