//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/preferences/PreferencePageRegistry.h>
#include <editor/core/property/PropertyDescriptor.h>
#include <editor/shell/ModalDialog.h>
#include <editor/shell/UiSkin.h>

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
    // (undoable); OK/Apply persist, Cancel reverts to the snapshot taken on open, Reset restores
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
        // Called after a successful commit so the host can persist.
        void SetOnApplied(std::function<void()> callback)
        {
            onApplied = std::move(callback);
        }

        void Open();
        void Close();

        void SetPageIndex(int index);
        int  GetPageIndex() const
        {
            return pageIndex;
        }
        int GetPageCount() const;

        // Layout queries (public for hosts and tests).
        sky::ui::UIRect PanelRect() const;
        sky::ui::UIRect ContentRect() const;
        sky::ui::UIRect CategoryRect(int index) const;
        int             CategoryAt(float x, float y) const;
        sky::ui::UIRect ButtonRect(int index) const; // 0 OK, 1 Cancel, 2 Apply, 3 Reset
        int             ButtonAt(float x, float y) const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    private:
        sky::ui::UIRect PageRect() const;
        void            BindPage(int index);
        void            SnapshotLive();
        void            RestoreSnapshot();
        void            Commit(); // copy nothing; just persist the live objects
        void            ResetPage();
        bool            IsEditable() const;

        sky::ui::UITextSystem  *textSystem = nullptr;
        UiSkin                  skin;
        PreferencePageRegistry *registry = nullptr;
        std::function<void()>   onApplied;

        ReflectedFormView *form = nullptr; // renders/edits the selected page (child element)

        int                               pageIndex     = 0;
        int                               hoverCategory = -1;
        int                               hoverButton   = -1;
        std::vector<std::vector<uint8_t>> snapshot; // live objects, captured on open/apply
    };

} // namespace sky::editor
