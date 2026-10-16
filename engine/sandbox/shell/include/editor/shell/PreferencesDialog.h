//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/preferences/PreferencesSession.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <editor/shell/ModalDialog.h>
#include <editor/widgets/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class ReflectedFormView;

    // Modal Preferences dialog: a category list (registered pages) on the left and the selected
    // page rendered by a ReflectedFormView on the right. Edits go through the reflected form
    // (undoable); OK/Apply write the owned working copies, Cancel discards them, Reset restores
    // the page's type defaults. See docs/editor/preferences-and-settings.md.
    class PreferencesDialog : public ModalDialog {
    public:
        explicit PreferencesDialog(sky::ui::UITextSystem *text);
        ~PreferencesDialog() override;

        const char *GetTypeName() const override
        {
            return "PreferencesDialog";
        }

        void SetModel(PreferencePageRegistry *inRegistry);
        // Notification after complete success. Legacy hosts may persist here;
        // opt-in persistence below gates success and reports failures instead.
        void SetOnApplied(std::function<void()> callback)
        {
            onApplied = std::move(callback);
        }

        void SetPersistence(std::shared_ptr<PreferencesPersistence> value)
        {
            if (persistence != value) {
                ++generation;
                persistence = std::move(value);
            }
        }
        const PreferenceResult &GetLastApplyResult() const
        {
            return lastApplyResult;
        }
        void SetOnApplyResult(std::function<void(const PreferenceResult &)> callback)
        {
            onApplyResult = std::move(callback);
        }

        void            Open();
        void            Close(); // discard changes since the last Apply
        void            Apply();
        CommandService &History()
        {
            return session->History();
        }

        void SetPageIndex(int index);
        int  GetPageIndex() const
        {
            return pageIndex;
        }
        int GetPageCount() const;

        // Consume model/theme changes and arrange before painting or input.
        // Geometry queries use the same path, including before the first paint.
        void SynchronizeLayout();

        // Layout queries (public for hosts and tests).
        sky::ui::UIRect PanelRect() const;
        sky::ui::UIRect ContentRect() const;
        sky::ui::UIRect CategoryViewportRect() const;
        sky::ui::UIRect PageViewportRect() const;
        sky::ui::UIRect CategoryRect(int index) const;
        int             CategoryAt(float x, float y) const;
        sky::ui::UIRect ButtonRect(int index) const; // 0 OK, 1 Cancel, 2 Apply, 3 Reset
        int             ButtonAt(float x, float y) const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        void                   OnPointerCancel(uint32_t pointerId) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &content) override;

    private:
        struct Layout;
        std::unique_ptr<Layout> composition;
        sky::ui::UIRect         ComputePanelRect() const;
        void                    UpdateComposition();
        void                    UpdatePresentation();
        sky::ui::UIRect         PageRect() const;
        void                    BindPage(int index);
        void                    SynchronizePages();
        void                    DrainPending();
        bool                    IsTransitionBusy() const;
        std::function<void()>   CloseState();
        void                    Commit(bool close);
        void                    ResetPage();
        bool                    IsEditable() const;

        sky::ui::UITextSystem                        *textSystem = nullptr;
        UiSkin                                        skin;
        PreferencePageRegistry                       *registry = nullptr;
        std::function<void()>                         onApplied;
        std::shared_ptr<PreferencesPersistence>       persistence;
        PreferenceResult                              lastApplyResult;
        std::function<void(const PreferenceResult &)> onApplyResult;

        // Declared before child destruction is forced in ~PreferencesDialog.
        std::shared_ptr<PreferencesSession> session = std::make_shared<PreferencesSession>();
        enum class Pending { None, Open, Close, Model };
        Pending                 pending         = Pending::None;
        PreferencePageRegistry *pendingRegistry = nullptr;
        bool                    transitioning   = false;
        uint64_t                generation      = 0;
        std::shared_ptr<void>   lifetime        = std::make_shared<int>(0);

        ReflectedFormView *form = nullptr; // renders/edits the selected page (child element)

        int      pageIndex     = 0;
        int      hoverCategory = -1;
        int      hoverButton   = -1;
        uint64_t themeRevision = 0;
        bool     layoutBusy    = false;
        bool     formCaptured  = false;
        uint32_t formPointerId = 0;
    };

} // namespace sky::editor
