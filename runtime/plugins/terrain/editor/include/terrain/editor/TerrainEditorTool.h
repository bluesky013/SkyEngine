//
// Created by blues on 2024/11/28.
//

#pragma once

#include <editor/framework/EditorToolBase.h>

namespace sky::editor {

    class TerrainEditorTool : public ToolBase {
    public:
        TerrainEditorTool() = default;
        ~TerrainEditorTool() override = default;

    private:
        bool Init() override;
        void Shutdown() override;
        ToolWidget *InitToolWidget(QWidget*) override;
    };

} // sky::editor
