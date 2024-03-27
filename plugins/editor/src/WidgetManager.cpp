//
// Created by blues on 2024/3/17.
//

#include <editor/WidgetManager.h>

namespace sky::editor {

    void WidgetManager::RegisterWidget(const std::string &key, IWidget* widget)
    {
        widgets.emplace(key, widget);
    }

    void WidgetManager::RemoveWidget(const std::string &key)
    {
        widgets.erase(key);
    }

    void WidgetManager::Render()
    {

    }

} // namespace sky::editor