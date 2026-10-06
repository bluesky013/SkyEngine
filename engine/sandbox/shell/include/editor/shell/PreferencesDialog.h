//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/preferences/PreferenceStore.h>
#include <editor/core/text/TextEditState.h>
#include <editor/shell/ModalDialog.h>
#include <editor/shell/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Engine-drawn modal Preferences dialog: a category list (registry pages) on
    // the left and the selected page on the right, editing a working copy of the
    // PreferenceStore. OK/Apply commit, Cancel reverts, Reset restores the page.
    class PreferencesDialog : public ModalDialog {
    public:
        explicit PreferencesDialog(sky::ui::UITextSystem *text);
        ~PreferencesDialog() override = default;

        const char *GetTypeName() const override
        {
            return "PreferencesDialog";
        }

        void SetModel(PreferenceRegistry *registry, PreferenceStore *store);
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
        struct Row {
            const PreferenceEntry *entry = nullptr;
            sky::ui::UIRect        bounds;
            sky::ui::UIRect        label;
            sky::ui::UIRect        control;
        };

        void       BuildRows(std::vector<Row> &rows) const;
        const Row *RowAt(float x, float y, std::vector<Row> &rows) const;
        void       Activate(const Row &row, float x, float y);
        void       SetSlider(const Row &row, float x, int channel);
        void       CycleCombo(const Row &row);
        void       FocusText(const Row &row, float x);

        void Commit();
        void Cancel();

        sky::ui::UITextSystem *textSystem = nullptr;
        UiSkin                 skin;
        PreferenceRegistry    *registry = nullptr;
        PreferenceStore       *store    = nullptr;
        std::function<void()>  onApplied;

        int           pageIndex     = 0;
        int           hoverCategory = -1;
        int           hoverButton   = -1;
        std::string   dragKey;
        int           dragChannel = -1;
        std::string   textKey;
        TextEditState textEdit;
    };

} // namespace sky::editor
