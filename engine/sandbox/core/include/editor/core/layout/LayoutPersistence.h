//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <string>
#include <vector>

namespace sky::editor {

    // Headless layout file persistence. The default path comes from the platform
    // user-config directory.
    class LayoutPersistence {
    public:
        LayoutPersistence() = delete;

        static bool Save(const LayoutModel &model, const std::string &path);
        static bool Load(const std::string &path, LayoutModel &model, const PanelRegistry *registry = nullptr,
                         std::vector<std::string> *warnings = nullptr);

        // <user-config>/editor_layout.json (empty when the platform has no path).
        static std::string GetDefaultPath();
    };

} // namespace sky::editor
