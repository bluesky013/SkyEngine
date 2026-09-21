//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/layout/PanelRegistry.h>

namespace sky::editor {

    // Registers the default editor panel ids. Views arrive with the UI pass;
    // the core only needs the ids, titles, and minimum sizes.
    void RegisterDefaultEditorPanels(PanelRegistry &registry);

} // namespace sky::editor
