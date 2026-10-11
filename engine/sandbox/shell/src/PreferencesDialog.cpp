//
// Created on 2026/10/06.
//

#include <editor/shell/PreferencesDialog.h>

#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>

#include <core/type/Rtti.h>
#include <framework/serialization/SerializationFactory.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <cstring>
#include <memory>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }

        std::size_t TypeSize(const PreferencePage &page)
        {
            if (page.type == nullptr || page.type->info == nullptr || page.type->info->staticInfo == nullptr) {
                return 0;
            }
            return page.type->info->staticInfo->size;
        }
    } // namespace

    PreferencesDialog::PreferencesDialog(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetVisible(false);
        form = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "")));
        form->SetHeaderVisible(false);
        form->SetVisible(false);
    }

    PreferencesDialog::~PreferencesDialog() = default;

    void PreferencesDialog::SetModel(PreferencePageRegistry *inRegistry)
    {
        registry = inRegistry;
    }

    int PreferencesDialog::GetPageCount() const
    {
        return registry != nullptr ? static_cast<int>(registry->GetPages().size()) : 0;
    }

    bool PreferencesDialog::IsEditable() const
    {
        return registry != nullptr && pageIndex >= 0 && pageIndex < GetPageCount();
    }

    void PreferencesDialog::Open()
    {
        hoverCategory = -1;
        hoverButton   = -1;
        pageIndex     = 0;
        SnapshotLive();
        ModalDialog::Open();
        if (form != nullptr) {
            form->SetVisible(true);
        }
        BindPage(pageIndex);
    }

    void PreferencesDialog::Close()
    {
        if (form != nullptr) {
            form->SetVisible(false);
        }
        ModalDialog::Close();
    }

    void PreferencesDialog::SetPageIndex(int index)
    {
        if (index < 0 || index >= GetPageCount()) {
            return;
        }
        pageIndex = index;
        BindPage(index);
        MarkPaintDirty();
    }

    void PreferencesDialog::BindPage(int index)
    {
        if (form == nullptr || registry == nullptr || index < 0 || index >= GetPageCount()) {
            return;
        }
        const PreferencePage &page = registry->GetPages()[static_cast<std::size_t>(index)];
        form->Bind(PropertyObject{page.object, page.type});
        MarkPaintDirty();
    }

    void PreferencesDialog::SnapshotLive()
    {
        snapshot.clear();
        if (registry == nullptr) {
            return;
        }
        for (const PreferencePage &page : registry->GetPages()) {
            const std::size_t    size = TypeSize(page);
            std::vector<uint8_t> bytes(size, 0);
            if (size != 0 && page.object != nullptr) {
                std::memcpy(bytes.data(), page.object, size);
            }
            snapshot.push_back(std::move(bytes));
        }
    }

    void PreferencesDialog::RestoreSnapshot()
    {
        if (registry == nullptr) {
            return;
        }
        const auto &pages = registry->GetPages();
        for (std::size_t i = 0; i < pages.size() && i < snapshot.size(); ++i) {
            if (pages[i].object != nullptr && !snapshot[i].empty()) {
                std::memcpy(pages[i].object, snapshot[i].data(), snapshot[i].size());
            }
        }
        BindPage(pageIndex);
    }

    void PreferencesDialog::Commit()
    {
        if (onApplied) {
            onApplied();
        }
        SnapshotLive(); // Cancel after Apply reverts to the applied state
    }

    void PreferencesDialog::ResetPage()
    {
        if (form == nullptr) {
            return;
        }
        ReflectedForm  &reflected = form->Form();
        CommandService &commands  = form->Commands();
        for (FormSection &section : reflected.GetSections()) {
            for (PropertyField &field : section.fields) {
                reflected.ResetToDefault(field, commands);
            }
        }
        form->Refresh();
    }

    sky::ui::UIRect PreferencesDialog::PanelRect() const
    {
        return CenteredPanel(M().preferencesPanelWidth, M().preferencesPanelHeight, 420.0f, 320.0f);
    }

    sky::ui::UIRect PreferencesDialog::ContentRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        return sky::ui::UIRect{panel.left, panel.top + M().titleBarHeight, panel.right, panel.bottom - M().footerHeight};
    }

    sky::ui::UIRect PreferencesDialog::CategoryRect(int index) const
    {
        const sky::ui::UIRect content = ContentRect();
        const float           top     = content.top + M().itemSpacing + static_cast<float>(index) * M().categoryRowHeight;
        return sky::ui::UIRect{content.left + M().itemSpacing, top, content.left + M().categoryWidth - M().itemSpacing,
                               top + M().categoryRowHeight - M().checkboxPad};
    }

    int PreferencesDialog::CategoryAt(float x, float y) const
    {
        for (int i = 0; i < GetPageCount(); ++i) {
            if (CategoryRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    sky::ui::UIRect PreferencesDialog::ButtonRect(int index) const
    {
        const sky::ui::UIRect panel     = PanelRect();
        const float           top       = panel.bottom - M().footerHeight + 11.0f;
        const float           rightEdge = panel.right - M().dialogMargin - static_cast<float>(index) * (M().buttonMinWidth + M().itemSpacing);
        return sky::ui::UIRect{rightEdge - M().buttonMinWidth, top, rightEdge, top + M().frameHeight};
    }

    int PreferencesDialog::ButtonAt(float x, float y) const
    {
        for (int i = 0; i < 4; ++i) {
            if (ButtonRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    sky::ui::UIRect PreferencesDialog::PageRect() const
    {
        const sky::ui::UIRect content = ContentRect();
        return sky::ui::UIRect{content.left + M().categoryWidth, content.top + M().itemSpacing, content.right - M().dialogMargin,
                               content.bottom - M().itemSpacing};
    }

    void PreferencesDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen || registry == nullptr) {
            return;
        }

        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = PanelRect();

        PaintBackdrop(context);
        const sky::ui::UIRect body = skin.DrawPanel(context, panel, "Preferences", true);

        // Category list.
        const sky::ui::UIRect cats{body.left, body.top, body.left + M().categoryWidth, body.bottom - M().footerHeight + 11.0f};
        uc::Fill(context, cats, th.colors.section);
        const auto &pages = registry->GetPages();
        for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
            const sky::ui::UIRect rect = CategoryRect(i);
            const RowState        state =
                (i == pageIndex) ? RowState::Selected : (i == hoverCategory ? RowState::Hover : (i % 2 ? RowState::Alt : RowState::Normal));
            skin.DrawRow(context, rect, state);
            uc::Text(context, pages[static_cast<std::size_t>(i)].title, th.fonts.label,
                     sky::ui::UIRect{rect.left + th.metrics.padX, rect.top, rect.right, rect.bottom}, th.colors.text, textSystem, uc::HAlign::Left,
                     uc::VAlign::Middle, true);
        }

        // Page content: the reflected form renders the selected page (rows built from reflection).
        if (form != nullptr) {
            form->SetBounds(PageRect());
        }

        // Footer buttons.
        const sky::ui::UIRect footer{panel.left, panel.bottom - M().footerHeight, panel.right, panel.bottom};
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);
        const char *labels[4] = {"OK", "Cancel", "Apply", "Reset"};
        for (int i = 0; i < 4; ++i) {
            const sky::ui::UIRect btn     = ButtonRect(i);
            const bool            primary = (i == 0);
            const bool            hovered = (hoverButton == i);
            if (primary) {
                uc::RoundedField(context, btn, hovered ? th.colors.accentHover : th.colors.accent, th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, labels[i], th.fonts.value, btn, th.colors.textOnAccent, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
            } else {
                skin.DrawToolItem(context, btn, labels[i], hovered);
            }
        }
    }

    sky::ui::UIEventResult PreferencesDialog::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        // A form popup (enum dropdown / color picker) extends beyond its bounds: deliver to it first.
        if (form != nullptr && form->WantsPointerCapture()) {
            return form->OnPointerEvent(event);
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            hoverCategory = CategoryAt(event.x, event.y);
            hoverButton   = ButtonAt(event.x, event.y);
            if (form != nullptr && PageRect().Contains(event.x, event.y)) {
                form->OnPointerEvent(event);
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::UP) {
            if (form != nullptr) {
                form->OnPointerEvent(event);
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::HANDLED;
        }

        const int button = ButtonAt(event.x, event.y);
        if (button >= 0) {
            if (button == 0) {
                Commit();
                Close();
            } else if (button == 1) {
                RestoreSnapshot();
                Close();
            } else if (button == 2) {
                Commit();
            } else if (button == 3) {
                ResetPage();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        const int category = CategoryAt(event.x, event.y);
        if (category >= 0) {
            SetPageIndex(category);
            return sky::ui::UIEventResult::HANDLED;
        }

        if (form != nullptr && PageRect().Contains(event.x, event.y)) {
            form->OnPointerEvent(event);
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult PreferencesDialog::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (!isOpen || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (form != nullptr && form->OnKeyEvent(event) == sky::ui::UIEventResult::HANDLED) {
            return sky::ui::UIEventResult::HANDLED;
        }
        constexpr uint32_t kEscape = 0x1B;
        if (event.keyCode == kEscape) {
            RestoreSnapshot();
            Close();
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult PreferencesDialog::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (!isOpen || form == nullptr) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        return form->OnTextInput(event);
    }

} // namespace sky::editor
