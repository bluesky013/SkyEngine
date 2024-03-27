//
// Created by Zach on 2024/3/17.
//

#pragma once

#include <editor/widgets/IWidget.h>
#include <unordered_map>

namespace sky::editor {

    class WidgetManager {
    public:
        WidgetManager() = default;
        ~WidgetManager() = default;

        void RegisterWidget(const std::string &key, IWidget*);
        void RemoveWidget(const std::string &key);

        void Render();

    private:
        std::unordered_map<std::string, IWidget*> widgets;
    };

} // namespace sky::editor
