//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/document/WorldDocument.h>
#include <editor/shell/PanelView.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>

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
        ~WorldConfigPanel() override;

        const char *GetTypeName() const override
        {
            return "WorldConfigPanel";
        }

        void SetTitleBarVisible(bool visible) override;

        // Hosts that own the document lifecycle can notify the panel directly.
        // This opts out of provider polling and allows unchanged layouts to be
        // retained. The document must outlive its binding (or be cleared first).
        void SetDocument(WorldDocument *document);
        // Rebind after an external mutation replaces or reallocates config data.
        // Abandons the former binding without rollback; call before subsequent
        // input/layout even when the WorldDocument pointer is unchanged.
        void RefreshDocument();
        // Synchronize external form revisions before an incremental layout pass.
        // The legacy provider constructor continues to poll its document here.
        void            SynchronizeLayout();
        sky::ui::UIRect ListRect() const;
        sky::ui::UIRect RowRect(int index) const;
        sky::ui::UIRect CheckRect(int index) const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &content) override;

    private:
        struct Layout;
        std::unique_ptr<Layout> layout;
        uint64_t                themeRevision = 0;
        bool                    layoutBusy    = false;
        void                    SynchronizeLayout(bool arranging);
        void                    UpdateComposition(bool arranging);
        sky::ui::UIRect         ComputeRowRect(int index) const;
        sky::ui::UIRect         ComputeCheckRect(int index) const;
        int                     RowAt(float x, float y) const;
        void                    Select(int index);
        void                    Rebind();
        void                    SynchronizeDocument();
        // Configuration is applied when the next Play world is built.
        void ToggleEnabled(int index);

        sky::ui::UITextSystem *textSystem = nullptr;
        UiSkin                 skin;
        DocumentProvider       documentProvider;
        std::string            title;
        bool                   titleVisible = true;

        ReflectedFormView *formView       = nullptr; // owned by this panel via AddChild
        WorldDocument     *lastDocument   = nullptr; // document backing the form's current binding
        bool               providerMode   = true;
        int                selected       = 0;
        uint64_t           configRevision = 0;
    };

} // namespace sky::editor
