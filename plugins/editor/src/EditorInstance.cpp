//
// Created by blues on 2024/3/17.
//

#include <editor/EditorInstance.h>

namespace sky::editor {

    void EditorInstance::Init()
    {
        wm = std::make_unique<WidgetManager>();
        wm->RegisterWidget(menuBar.GetName(), &menuBar);
    }

    void EditorInstance::Tick(float time)
    {
        wm->Render();
    }

} // namespace sky::editor