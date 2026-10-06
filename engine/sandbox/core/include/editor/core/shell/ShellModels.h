//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>

#include <cstddef>
#include <string>
#include <vector>

namespace sky::editor {

    // View-model for the shell's chrome, kept UI-free and headless-testable.

    // One "View" menu entry derived from the registry + current layout.
    struct ViewMenuItem {
        std::string panelId;
        std::string title;
        bool        shown = false;
    };

    // Registered panels with their current visibility, ordered by title.
    std::vector<ViewMenuItem> BuildViewMenuItems(const PanelRegistry &registry, const LayoutModel &layout);

    // Single-line status text (project / mode / RHI / selection / fps).
    std::string FormatStatusBar(const std::string &project, const std::string &mode, const std::string &rhi,
                                std::size_t selectionCount, float fps);

} // namespace sky::editor
