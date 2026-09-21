//
// Created on 2026/09/21.
//

#include <editor/core/layout/PanelRegistry.h>
#include <utility>

namespace sky::editor {

    void PanelRegistry::Register(PanelInfo info)
    {
        const std::string id = info.id;
        panels[id] = std::move(info);
    }

    bool PanelRegistry::Unregister(const std::string &id)
    {
        return panels.erase(id) > 0;
    }

    const PanelInfo *PanelRegistry::Find(const std::string &id) const
    {
        const auto iter = panels.find(id);
        return iter == panels.end() ? nullptr : &iter->second;
    }

} // namespace sky::editor
