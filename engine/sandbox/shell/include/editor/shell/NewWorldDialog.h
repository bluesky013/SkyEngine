//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/text/TextEditState.h>
#include <editor/shell/ModalDialog.h>
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

    // "New World" create dialog: a name + a location to create the world in.
    // Distinct from the file browser (which is for opening/picking).
    class NewWorldDialog : public ModalDialog {
    public:
        explicit NewWorldDialog(sky::ui::UITextSystem *text);
        ~NewWorldDialog() override = default;

        const char *GetTypeName() const override
        {
            return "NewWorldDialog";
        }

        // Path received by the callback is the full world file path.
        void SetOnCreate(std::function<void(const std::string &)> handler)
        {
            onCreate = std::move(handler);
        }

        void Open(const std::string &location, const std::string &name);
        void Close();

        sky::ui::UIRect PanelRect() const;
        sky::ui::UIRect NameFieldRect() const;
        sky::ui::UIRect LocationFieldRect() const;
        sky::ui::UIRect ButtonRect(int index) const; // 0 Create, 1 Cancel
        int             ButtonAt(float x, float y) const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    private:
        void                                     Accept();
        TextEditState                           *Focused();
        sky::ui::UITextSystem                   *textSystem = nullptr;
        UiSkin                                   skin;
        TextEditState                            nameEdit;
        TextEditState                            locationEdit;
        int                                      focus = 0; // 0 = name, 1 = location
        std::function<void(const std::string &)> onCreate;
    };

} // namespace sky::editor
