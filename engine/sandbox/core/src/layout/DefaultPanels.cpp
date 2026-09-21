//
// Created on 2026/09/21.
//

#include <editor/core/layout/DefaultPanels.h>

namespace sky::editor {

    void RegisterDefaultEditorPanels(PanelRegistry &registry)
    {
        registry.Register(PanelInfo{"viewport", "Viewport", 320.f, 240.f, nullptr});
        registry.Register(PanelInfo{"outliner", "Outliner", 200.f, 120.f, nullptr});
        registry.Register(PanelInfo{"inspector", "Inspector", 260.f, 160.f, nullptr});
        registry.Register(PanelInfo{"outputlog", "Output Log", 240.f, 120.f, nullptr});
        registry.Register(PanelInfo{"console", "Console", 240.f, 120.f, nullptr});
    }

} // namespace sky::editor
