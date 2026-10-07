//
// Created on 2026/10/07.
//

#include <editor/shell/NewWorldDialog.h>

#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <filesystem>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }
    } // namespace

    NewWorldDialog::NewWorldDialog(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetVisible(false);
    }

    void NewWorldDialog::Open(const std::string &location, const std::string &name)
    {
        locationEdit.SetText(location);
        nameEdit.SetText(name);
        focus = 0;
        ModalDialog::Open();
    }

    void NewWorldDialog::Close()
    {
        ModalDialog::Close();
    }

    TextEditState *NewWorldDialog::Focused()
    {
        return focus == 1 ? &locationEdit : &nameEdit;
    }

    sky::ui::UIRect NewWorldDialog::PanelRect() const
    {
        return CenteredPanel(M().newWorldPanelWidth, M().newWorldPanelHeight, 380.0f, 200.0f);
    }

    sky::ui::UIRect NewWorldDialog::NameFieldRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        return sky::ui::UIRect{panel.left + M().dialogMargin + M().formLabelWidth, panel.top + M().headerHeight, panel.right - M().dialogMargin,
                               panel.top + M().headerHeight + M().frameHeight};
    }

    sky::ui::UIRect NewWorldDialog::LocationFieldRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        const float           top   = panel.top + M().headerHeight + M().rowHeight;
        return sky::ui::UIRect{panel.left + M().dialogMargin + M().formLabelWidth, top, panel.right - M().dialogMargin, top + M().frameHeight};
    }

    sky::ui::UIRect NewWorldDialog::ButtonRect(int index) const
    {
        const sky::ui::UIRect panel     = PanelRect();
        const float           top       = panel.bottom - M().footerHeight + M().cellPadding * 2.0f;
        const float           gap       = M().itemSpacing;
        const float           rightEdge = panel.right - M().dialogMargin - static_cast<float>(1 - index) * (M().buttonMinWidth + gap);
        return sky::ui::UIRect{rightEdge - M().buttonMinWidth, top, rightEdge, top + M().frameHeight};
    }

    int NewWorldDialog::ButtonAt(float x, float y) const
    {
        for (int i = 0; i < 2; ++i) {
            if (ButtonRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    void NewWorldDialog::Accept()
    {
        const std::string name = nameEdit.GetText();
        if (name.empty()) {
            return;
        }
        const std::filesystem::path path = std::filesystem::path(locationEdit.GetText()) / (name + ".world");
        Close();
        if (onCreate) {
            onCreate(path.string());
        }
    }

    void NewWorldDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen) {
            return;
        }
        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = PanelRect();

        PaintBackdrop(context);
        skin.DrawPanel(context, panel, "New World", true);

        const auto label = [&](const char *text, const sky::ui::UIRect &row) {
            uc::Text(context, text, th.fonts.label,
                     sky::ui::UIRect{panel.left + M().dialogMargin, row.top, panel.left + M().dialogMargin + M().formLabelWidth, row.bottom},
                     th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        };

        const sky::ui::UIRect nameField = NameFieldRect();
        label("Name", nameField);
        skin.DrawField(context, nameField, focus == 0, false);
        uc::Text(context, nameEdit.GetText(), th.fonts.value,
                 sky::ui::UIRect{nameField.left + th.metrics.controlPad, nameField.top, nameField.right, nameField.bottom}, th.colors.text,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);

        const sky::ui::UIRect locField = LocationFieldRect();
        label("Location", locField);
        skin.DrawField(context, locField, focus == 1, false);
        uc::Text(context, locationEdit.GetText(), th.fonts.value,
                 sky::ui::UIRect{locField.left + th.metrics.controlPad, locField.top, locField.right, locField.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, true);

        const sky::ui::UIRect footer{panel.left, panel.bottom - M().footerHeight, panel.right, panel.bottom};
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);
        const char *labels[2] = {"Create", "Cancel"};
        for (int i = 0; i < 2; ++i) {
            const sky::ui::UIRect btn     = ButtonRect(i);
            const bool            primary = (i == 0);
            if (primary) {
                uc::RoundedField(context, btn, th.colors.accent, th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, labels[i], th.fonts.value, btn, th.colors.textOnAccent, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
            } else {
                skin.DrawToolItem(context, btn, labels[i], false);
            }
        }
    }

    sky::ui::UIEventResult NewWorldDialog::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!isOpen || event.action != sky::ui::UIPointerAction::DOWN) {
            return isOpen ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }
        const int button = ButtonAt(event.x, event.y);
        if (button == 0) {
            Accept();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (button == 1) {
            Close();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (NameFieldRect().Contains(event.x, event.y)) {
            focus = 0;
            MarkPaintDirty();
        } else if (LocationFieldRect().Contains(event.x, event.y)) {
            focus = 1;
            MarkPaintDirty();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult NewWorldDialog::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (!isOpen || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        constexpr uint32_t kEscape = 0x1B;
        constexpr uint32_t kReturn = 0x0D;
        constexpr uint32_t kTab    = 0x09;

        if (event.keyCode == kEscape) {
            Close();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kReturn) {
            Accept();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kTab) {
            focus = focus == 0 ? 1 : 0;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }
        const bool shift = (event.modifiers & kModShift) != 0;
        const bool ctrl  = (event.modifiers & kModCtrl) != 0;
        if (Focused()->OnKey(event.keyCode, shift, ctrl)) {
            MarkPaintDirty();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult NewWorldDialog::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (Focused()->OnText(event.text)) {
            MarkPaintDirty();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

} // namespace sky::editor
