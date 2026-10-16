//
// Created on 2026/10/07.
//

#include <editor/shell/NewWorldDialog.h>

#include <editor/core/input/KeyModifiers.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }
    } // namespace

    struct NewWorldDialog::Layout {
        sky::ui::DockPanel                 *panel   = nullptr;
        sky::ui::UIElement                 *header  = nullptr;
        sky::ui::DockPanel                 *body    = nullptr;
        sky::ui::GridPanel                 *rows    = nullptr;
        sky::ui::DockPanel                 *footer  = nullptr;
        sky::ui::GridPanel                 *buttons = nullptr;
        std::array<sky::ui::DockPanel *, 2> row{};
        std::array<sky::ui::DockPanel *, 2> buttonCell{};
        std::array<sky::ui::UIElement *, 2> label{}, field{}, button{};
    };

    NewWorldDialog::NewWorldDialog(sky::ui::UITextSystem *text)
        : layout(std::make_unique<Layout>()), textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        using namespace sky::ui;
        const auto slot = [] {
            auto value = std::make_unique<UIElement>();
            value->SetLayoutVolatile(false);
            return value;
        };
        SetLayoutVolatile(false);
        SetVisible(false);
        layout->panel   = static_cast<DockPanel *>(AddChild(std::make_unique<DockPanel>()));
        layout->header  = layout->panel->Add(slot(), {DockSide::Top});
        layout->footer  = static_cast<DockPanel *>(layout->panel->Add(std::make_unique<DockPanel>(), {DockSide::Bottom}));
        layout->buttons = static_cast<GridPanel *>(layout->footer->Add(std::make_unique<GridPanel>(), {DockSide::Right}));
        layout->buttons->SetGrid(1, 2);
        layout->body = static_cast<DockPanel *>(layout->panel->Add(std::make_unique<DockPanel>()));
        layout->rows = static_cast<GridPanel *>(layout->body->Add(std::make_unique<GridPanel>(), {DockSide::Top}));
        layout->rows->SetGrid(2, 1);
        for (int i = 0; i < 2; ++i) {
            layout->row[i]        = static_cast<DockPanel *>(layout->rows->Add(std::make_unique<DockPanel>(), {i, 0}));
            layout->label[i]      = layout->row[i]->Add(slot(), {DockSide::Left});
            layout->field[i]      = layout->row[i]->Add(slot());
            layout->buttonCell[i] = static_cast<DockPanel *>(layout->buttons->Add(std::make_unique<DockPanel>(), {0, i}));
            layout->button[i]     = layout->buttonCell[i]->Add(slot());
        }
    }

    NewWorldDialog::~NewWorldDialog() = default;

    void NewWorldDialog::SynchronizeLayout()
    {
        if (layoutBusy)
            return;
        layoutBusy = true;
        SynchronizeDefaultUiTheme(*this, themeRevision);
        const auto &m       = M();
        const auto  surface = GetArrangedSize();
        const float margin  = std::max(0.0f, m.dialogMargin);
        const float width   = std::min(std::max(0.0f, m.newWorldPanelWidth), std::max(0.0f, surface.width - 2.0f * margin));
        const float height  = std::min(std::max(0.0f, m.newWorldPanelHeight), std::max(0.0f, surface.height - 2.0f * margin));
        const auto  bounds  = GetBounds();
        const float left    = bounds.left + (surface.width - width) * 0.5f;
        const float top     = bounds.top + (surface.height - height) * 0.5f;
        const float header  = std::min(std::max(0.0f, m.panelHeaderHeight), height);
        const float footer  = std::min(std::max(std::max(0.0f, m.footerHeight), m.frameHeight + 2.0f * m.cellPadding), height - header);
        layout->panel->SetSlot(layout->header, {sky::ui::DockSide::Top, header});
        layout->panel->SetSlot(layout->footer, {sky::ui::DockSide::Bottom, footer});
        const auto padding = [](sky::ui::UIElement &element, float horizontal, float vertical) {
            auto value        = element.GetLayout();
            value.paddingLeft = value.paddingRight = horizontal;
            value.paddingTop = value.paddingBottom = vertical;
            element.SetLayout(value);
        };
        const float bodyHeight = height - header - footer;
        const float padX       = std::min(margin, width * 0.5f);
        const float padY       = std::min(std::max(0.0f, m.cellPadding), bodyHeight * 0.5f);
        padding(*layout->body, padX, padY);
        const float rowsHeight =
            std::min(std::max(0.0f, bodyHeight - 2.0f * padY), 2.0f * (std::max(0.0f, m.frameHeight) + std::max(0.0f, m.itemSpacing)));
        layout->body->SetSlot(layout->rows, {sky::ui::DockSide::Top, rowsHeight});
        const float rowWidth = std::max(0.0f, width - 2.0f * padX);
        for (int i = 0; i < 2; ++i) {
            padding(*layout->row[i], 0.0f, std::min(std::max(0.0f, m.itemSpacing) * 0.5f, rowsHeight * 0.25f));
            layout->row[i]->SetSlot(layout->label[i], {sky::ui::DockSide::Left, std::min(std::max(0.0f, m.formLabelWidth), rowWidth * 0.5f)});
        }
        padding(*layout->footer, padX, std::min(std::max(0.0f, m.cellPadding), footer * 0.5f));
        layout->footer->SetSlot(layout->buttons, {sky::ui::DockSide::Right,
                                                  std::min(rowWidth, 2.0f * (std::max(0.0f, m.buttonMinWidth) + std::max(0.0f, m.itemSpacing)))});
        const float cellWidth = std::min(rowWidth * 0.5f, std::max(0.0f, m.buttonMinWidth) + std::max(0.0f, m.itemSpacing));
        for (auto *cell : layout->buttonCell) {
            padding(*cell, std::min(std::max(0.0f, m.itemSpacing) * 0.5f, cellWidth * 0.5f), 0.0f);
        }
        layout->panel->ArrangeIfNeeded({left, top, left + width, top + height}, {width, height});
        layoutBusy = false;
    }

    void NewWorldDialog::ArrangeChildren(const sky::ui::UIRect &)
    {
        SynchronizeLayout();
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
        const_cast<NewWorldDialog *>(this)->SynchronizeLayout();
        return layout->panel->GetBounds();
    }

    sky::ui::UIRect NewWorldDialog::NameFieldRect() const
    {
        const_cast<NewWorldDialog *>(this)->SynchronizeLayout();
        return layout->field[0]->GetBounds();
    }

    sky::ui::UIRect NewWorldDialog::LocationFieldRect() const
    {
        const_cast<NewWorldDialog *>(this)->SynchronizeLayout();
        return layout->field[1]->GetBounds();
    }

    sky::ui::UIRect NewWorldDialog::ButtonRect(int index) const
    {
        const_cast<NewWorldDialog *>(this)->SynchronizeLayout();
        return index >= 0 && index < 2 ? layout->button[index]->GetBounds() : sky::ui::UIRect{};
    }

    int NewWorldDialog::ButtonAt(float x, float y) const
    {
        for (int i = 0; i < 2; ++i) {
            const auto rect = ButtonRect(i);
            if (rect.Width() > 0.0f && rect.Height() > 0.0f && rect.Contains(x, y)) {
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
        const std::filesystem::path path   = std::filesystem::path(locationEdit.GetText()) / (name + ".world");
        const auto                  create = onCreate;
        Close();
        if (create) {
            create(path.string());
        }
    }

    void NewWorldDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen) {
            return;
        }
        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = layout->panel->GetBounds();

        PaintBackdrop(context);
        skin.DrawPanel(context, panel, "New World", true);

        const auto label = [&](const char *text, int index) {
            uc::Text(context, text, th.fonts.label, layout->label[index]->GetBounds(), th.colors.textMuted, textSystem, uc::HAlign::Left,
                     uc::VAlign::Middle, true);
        };

        const sky::ui::UIRect nameField = layout->field[0]->GetBounds();
        label("Name", 0);
        skin.DrawField(context, nameField, focus == 0, false);
        uc::Text(context, nameEdit.GetText(), th.fonts.value,
                 sky::ui::UIRect{nameField.left + th.metrics.controlPad, nameField.top, nameField.right, nameField.bottom}, th.colors.text,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);

        const sky::ui::UIRect locField = layout->field[1]->GetBounds();
        label("Location", 1);
        skin.DrawField(context, locField, focus == 1, false);
        uc::Text(context, locationEdit.GetText(), th.fonts.value,
                 sky::ui::UIRect{locField.left + th.metrics.controlPad, locField.top, locField.right, locField.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, true);

        const sky::ui::UIRect footer = layout->footer->GetBounds();
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);
        const char *labels[2] = {"Create", "Cancel"};
        for (int i = 0; i < 2; ++i) {
            const sky::ui::UIRect btn     = layout->button[i]->GetBounds();
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
