//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/document/WorldDocument.h>
#include <editor/shell/PanelView.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <string>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Dockable "Config" panel: a left list of world subsystems and, on the right,
    // a reflected-form view (the same system as the Inspector) bound to the
    // selected subsystem's config. World config is project state (not user
    // Preferences). The form view is a child created once at construction; the
    // panel only binds/positions it, so nothing mutates the tree during paint.
    class WorldConfigPanel : public sky::ui::UIElement, public IPanelChrome {
    public:
        using DocumentProvider = std::function<WorldDocument *()>;

        WorldConfigPanel(sky::ui::UITextSystem *text, DocumentProvider provider, std::string title);
        ~WorldConfigPanel() override = default;

        const char *GetTypeName() const override
        {
            return "WorldConfigPanel";
        }

        void SetTitleBarVisible(bool visible) override;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    private:
        sky::ui::UIRect ListRect() const;
        sky::ui::UIRect RowRect(int index) const;
        sky::ui::UIRect CheckRect(int index) const;
        int             RowAt(float x, float y) const;
        void            Select(int index);
        void            Rebind();
        // Toggles a subsystem's enabled flag (persisted + rebuilt immediately).
        void ToggleEnabled(int index);

        sky::ui::UITextSystem *textSystem = nullptr;
        UiSkin                 skin;
        DocumentProvider       documentProvider;
        std::string            title;
        bool                   titleVisible = true;

        ReflectedFormView *formView     = nullptr; // owned by this panel via AddChild
        WorldDocument     *lastDocument = nullptr; // detects open/close to rebind
        int                selected     = 0;
    };

} // namespace sky::editor
