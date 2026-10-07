//
// Created on 2026/10/06.
//

#include <editor/core/shell/ShellModels.h>

#include <algorithm>
#include <cstdio>

namespace sky::editor {

    std::vector<ViewMenuItem> BuildViewMenuItems(const PanelRegistry &registry, const LayoutModel &layout)
    {
        std::vector<std::string> present;
        layout.CollectPanels(present);

        std::vector<ViewMenuItem> items;
        items.reserve(registry.GetAll().size());
        for (const auto &entry : registry.GetAll()) {
            ViewMenuItem item;
            item.panelId = entry.first;
            item.title   = entry.second.title;
            item.shown   = std::find(present.begin(), present.end(), entry.first) != present.end();
            items.push_back(std::move(item));
        }
        std::sort(items.begin(), items.end(), [](const ViewMenuItem &a, const ViewMenuItem &b) { return a.title < b.title; });
        return items;
    }

    std::string FormatWindowTitle(const std::string &document, bool dirty, const std::string &project)
    {
        const std::string doc  = document.empty() ? std::string("Untitled") : document;
        const std::string mark = dirty ? "*" : "";
        return doc + mark + " - " + (project.empty() ? std::string("SkyEngine") : project) + " - SkyEngine Editor";
    }

    std::string FormatStatusBar(const std::string &document,
                                bool               dirty,
                                const std::string &project,
                                const std::string &mode,
                                const std::string &rhi,
                                std::size_t        selectionCount,
                                float              fps)
    {
        const std::string doc  = document.empty() ? std::string("Untitled") : document;
        const std::string mark = dirty ? "*" : "";
        std::string       text = doc + mark + "    " + project + "    " + mode + "    RHI: " + rhi;
        text += "    sel: " + std::to_string(selectionCount);
        char frame[48] = {0};
        std::snprintf(frame, sizeof(frame), "    %.0f fps", static_cast<double>(fps));
        text += frame;
        return text;
    }

} // namespace sky::editor
