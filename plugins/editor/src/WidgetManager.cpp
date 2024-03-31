//
// Created by blues on 2024/3/17.
//

#include <editor/WidgetManager.h>
#include <imgui.h>

namespace sky::editor {

    void WidgetManager::Execute(ImContext &context)
    {
        context.MakeCurrent();
        for (auto &widget : widgets) {
            widget.second->Execute(context);
        }
    }

    void WidgetManager::RegisterWidget(ImWidget* widget)
    {
        widgets.emplace(widget->GetName(), widget);
    }

    void WidgetManager::RemoveWidget(const std::string &key)
    {
        widgets.erase(key);
    }
} // namespace sky::editor