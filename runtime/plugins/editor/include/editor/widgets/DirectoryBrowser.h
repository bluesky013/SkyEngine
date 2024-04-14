//
// Created by blues on 2024/3/30.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>

namespace sky::editor {

    class DirectoryBrowser : public ImWidget, public IButtonEvent {
    public:
        explicit DirectoryBrowser() : ImWidget("Browser") {}
        ~DirectoryBrowser() override = default;

        void Execute(ImContext &context) override;
        void BindEvent(EventID id);

        void AddPath(const std::string &path) { rootPaths.emplace_back(path); }
    private:
        void OnClicked() override;

        std::vector<std::string> rootPaths;
        std::string extFilter;
        bool isOpen = false;

        EventBinder<IButtonEvent> binder;
    };

} // namespace sky::editor