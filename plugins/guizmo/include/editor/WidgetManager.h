//
// Created by Zach on 2024/3/17.
//

#pragma once

#include <imgui/ImGuiInstance.h>
#include <unordered_map>

namespace sky::editor {

    class WidgetManager : public ImWidget {
    public:
        WidgetManager() : ImWidget("parent") {}
        ~WidgetManager() override = default;

        void RegisterWidget(ImWidget*);
        void RemoveWidget(const std::string &key);

        void Execute(ImContext &context) override;

    private:
        std::unordered_map<std::string, std::unique_ptr<ImWidget>> widgets;
    };

} // namespace sky::editor
