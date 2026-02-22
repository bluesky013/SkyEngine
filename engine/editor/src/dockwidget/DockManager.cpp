//
// Created by Zach Lee on 2021/12/14.
//

#include <editor/dockwidget/DockManager.h>

namespace sky::editor {

    void DockManager::Register(uint32_t key, QDockWidget& dock)
    {
        docks.emplace(key, &dock);
    }

    void DockManager::UnRegister(uint32_t key)
    {
        auto iter = docks.find(key);
        if (iter != docks.end()) {
            docks.erase(iter);
        }
    }

    QDockWidget* DockManager::Find(uint32_t key) const
    {
        auto iter = docks.find(key);
        return iter == docks.end() ? nullptr : iter->second;
    }

}