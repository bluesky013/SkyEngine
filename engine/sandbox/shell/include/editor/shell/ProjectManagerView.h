//
// Created on 2026/10/05.
//

#pragma once

#include <ui/UIElement.h>

#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Project Manager (hub) view: recent projects + Add/New/Quit and per-selection
    // actions (Open / Remove from list / Delete folder). A pure sky::ui element
    // shown by the editor binary when no project is specified.
    class ProjectManagerView : public sky::ui::UIElement {
    public:
        void SetTextSystem(sky::ui::UITextSystem *text) { textSystem = text; }
        void SetEngineVersion(std::string version) { engineVersion = std::move(version); }
        void SetRecent(std::vector<std::string> paths);
        void SetStatus(std::string text)
        {
            status = std::move(text);
            MarkPaintDirty();
        }

        std::function<void(const std::string &)> onOpen;
        std::function<void(const std::string &)> onRemove; // remove from list
        std::function<void(const std::string &)> onDelete; // delete the project folder
        std::function<void()>                    onAdd;    // add existing project (browse)
        std::function<void()>                    onNew;
        std::function<void()>                    onQuit;

        const char *GetTypeName() const override { return "ProjectManagerView"; }

        void OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

        // 0 = Add, 1 = New, 2 = Quit  |  0 = Open, 1 = Remove, 2 = Delete
        sky::ui::UIRect TopButtonRect(int index) const;
        sky::ui::UIRect ActionRect(int index) const;
        sky::ui::UIRect RowRect(int index) const;
        int              TopButtonAt(float x, float y) const;
        int              ActionAt(float x, float y) const;
        int              RowAt(float x, float y) const;

    private:
        sky::ui::UITextSystem   *textSystem = nullptr;
        std::vector<std::string> recent;
        std::string              status;
        std::string              engineVersion;
        int                      selected = -1;
        int                      hoverRow = -1;
        int                      hoverTop = -1;
        int                      hoverAction = -1;
        bool                     confirmDelete = false;
        long long                lastClickMs = 0;
    };

} // namespace sky::editor
