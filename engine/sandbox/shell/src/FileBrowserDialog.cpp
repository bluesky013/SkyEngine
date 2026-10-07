//
// Created on 2026/10/06.
//

#include <editor/shell/FileBrowserDialog.h>

#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        constexpr float kPanelW     = 820.0f;
        constexpr float kPanelH     = 560.0f;
        constexpr float kMargin     = 12.0f;
        constexpr float kTitleH     = 36.0f;
        constexpr float kSidebarW   = 170.0f;
        constexpr float kToolbarH   = 30.0f;
        constexpr float kHeaderRowH = 22.0f;
        constexpr float kRowH       = 24.0f;
        constexpr float kFooterH    = 46.0f;
        constexpr float kBtnW       = 88.0f;
        constexpr float kBtnH       = 26.0f;
        constexpr float kItemH      = 22.0f;

        long long NowMs()
        {
            using clock = std::chrono::steady_clock;
            return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
        }

        std::string TypeLabelFor(const FileBrowserEntry &entry)
        {
            if (entry.isDirectory) {
                return "Folder";
            }
            const std::size_t dot = entry.name.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < entry.name.size()) {
                std::string ext = entry.name.substr(dot + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                return ext;
            }
            if (!entry.typeId.empty()) {
                return entry.typeId;
            }
            return "File";
        }

        RowState StateFor(int index, bool selected, bool hovered)
        {
            if (selected) {
                return RowState::Selected;
            }
            if (hovered) {
                return RowState::Hover;
            }
            return (index % 2) ? RowState::Alt : RowState::Normal;
        }
    } // namespace

    FileBrowserDialog::FileBrowserDialog(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetVisible(false);
    }

    void FileBrowserDialog::Open(const FileBrowserRequest &request)
    {
        model.SetRequest(request);
        filterPopupOpen  = false;
        hoverPlace       = -1;
        hoverRow         = -1;
        hoverFilterItem  = -1;
        hoverButton      = -1;
        hoverUp          = false;
        hoverNewFolder   = false;
        hoverFilter      = false;
        lastClickRow     = -1;
        lastClickMs      = 0;
        renameActive     = false;
        contextMenuOpen  = false;
        contextMenuRow   = -1;
        hoverContextItem = -1;
        nameEdit.SetText(model.GetName());
        ModalDialog::Open();
    }

    void FileBrowserDialog::Close()
    {
        filterPopupOpen = false;
        contextMenuOpen = false;
        renameActive    = false;
        ModalDialog::Close();
    }

    int FileBrowserDialog::NameCaretFromX(float x) const
    {
        const std::string &name = nameEdit.GetText();
        if (textSystem == nullptr) {
            return static_cast<int>(name.size());
        }
        const sky::ui::UIRect field  = NameFieldRect();
        float                 cursor = field.left;
        for (std::size_t i = 0; i < name.size(); ++i) {
            const float w = uc::TextWidth(name.substr(i, 1), skin.Theme().fonts.value, textSystem);
            if (x < cursor + w * 0.5f) {
                return static_cast<int>(i);
            }
            cursor += w;
        }
        return static_cast<int>(name.size());
    }

    float FileBrowserDialog::NameCaretX() const
    {
        const sky::ui::UIRect field = NameFieldRect();
        if (textSystem == nullptr) {
            return field.left;
        }
        const std::string &name  = nameEdit.GetText();
        const std::size_t  caret = std::min(nameEdit.GetCaret(), name.size());
        return field.left + uc::TextWidth(name.substr(0, caret), skin.Theme().fonts.value, textSystem);
    }

    void FileBrowserDialog::SyncName()
    {
        model.SetName(nameEdit.GetText());
        MarkPaintDirty();
    }

    // ---- layout -------------------------------------------------------------

    sky::ui::UIRect FileBrowserDialog::PanelRect() const
    {
        return CenteredPanel(kPanelW, kPanelH, 360.0f, 260.0f);
    }

    sky::ui::UIRect FileBrowserDialog::SidebarRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        return sky::ui::UIRect{panel.left, panel.top + kTitleH, panel.left + kSidebarW, panel.bottom - kFooterH};
    }

    sky::ui::UIRect FileBrowserDialog::PlaceRowRect(int index) const
    {
        const sky::ui::UIRect sidebar = SidebarRect();
        const float           top     = sidebar.top + 6.0f + static_cast<float>(index) * kItemH;
        return sky::ui::UIRect{sidebar.left + 6.0f, top, sidebar.right - 6.0f, top + kItemH - 2.0f};
    }

    sky::ui::UIRect FileBrowserDialog::ToolbarRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        const float           left  = panel.left + kSidebarW + kMargin;
        return sky::ui::UIRect{left, panel.top + kTitleH, panel.right - kMargin, panel.top + kTitleH + kToolbarH};
    }

    sky::ui::UIRect FileBrowserDialog::UpRect() const
    {
        const sky::ui::UIRect toolbar = ToolbarRect();
        return sky::ui::UIRect{toolbar.left + 2.0f, toolbar.top + 3.0f, toolbar.left + 34.0f, toolbar.bottom - 3.0f};
    }

    sky::ui::UIRect FileBrowserDialog::PathRect() const
    {
        const sky::ui::UIRect toolbar = ToolbarRect();
        const sky::ui::UIRect up      = UpRect();
        const float           right   = WriteMode() ? NewFolderRect().left - 6.0f : toolbar.right - 2.0f;
        return sky::ui::UIRect{up.right + 6.0f, toolbar.top + 3.0f, right, toolbar.bottom - 3.0f};
    }

    sky::ui::UIRect FileBrowserDialog::NewFolderRect() const
    {
        const sky::ui::UIRect toolbar = ToolbarRect();
        return sky::ui::UIRect{toolbar.right - 108.0f, toolbar.top + 3.0f, toolbar.right, toolbar.bottom - 3.0f};
    }

    sky::ui::UIRect FileBrowserDialog::ListHeaderRect() const
    {
        const sky::ui::UIRect toolbar = ToolbarRect();
        return sky::ui::UIRect{toolbar.left, toolbar.bottom, toolbar.right, toolbar.bottom + kHeaderRowH};
    }

    sky::ui::UIRect FileBrowserDialog::ListRect() const
    {
        const sky::ui::UIRect toolbar = ToolbarRect();
        const sky::ui::UIRect panel   = PanelRect();
        return sky::ui::UIRect{toolbar.left, toolbar.bottom + kHeaderRowH, toolbar.right, panel.bottom - kFooterH - 2.0f};
    }

    sky::ui::UIRect FileBrowserDialog::RowRect(int index) const
    {
        const sky::ui::UIRect list = ListRect();
        const float           top  = list.top + static_cast<float>(index) * kRowH;
        return sky::ui::UIRect{list.left, top, list.right, top + kRowH - 2.0f};
    }

    sky::ui::UIRect FileBrowserDialog::FooterRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        return sky::ui::UIRect{panel.left, panel.bottom - kFooterH, panel.right, panel.bottom};
    }

    sky::ui::UIRect FileBrowserDialog::FilterRect() const
    {
        const sky::ui::UIRect footer = FooterRect();
        return sky::ui::UIRect{footer.left + kMargin, footer.top + 10.0f, footer.left + kMargin + 220.0f, footer.top + 10.0f + kBtnH};
    }

    sky::ui::UIRect FileBrowserDialog::FilterItemRect(int index) const
    {
        const sky::ui::UIRect filter = FilterRect();
        const int             count  = FilterItemCount();
        const float           bottom = filter.top - 2.0f;
        const float           top    = bottom - static_cast<float>(count - index) * kItemH;
        return sky::ui::UIRect{filter.left, top, filter.right, top + kItemH};
    }

    sky::ui::UIRect FileBrowserDialog::NameFieldRect() const
    {
        const sky::ui::UIRect footer = FooterRect();
        const sky::ui::UIRect filter = FilterRect();
        return sky::ui::UIRect{filter.right + 16.0f, footer.top + 10.0f, filter.right + 16.0f + 220.0f, footer.top + 10.0f + kBtnH};
    }

    sky::ui::UIRect FileBrowserDialog::ButtonRect(int index) const
    {
        const sky::ui::UIRect footer    = FooterRect();
        const float           gap       = 8.0f;
        const float           rightEdge = footer.right - kMargin - static_cast<float>(1 - index) * (kBtnW + gap);
        return sky::ui::UIRect{rightEdge - kBtnW, footer.top + 10.0f, rightEdge, footer.top + 10.0f + kBtnH};
    }

    // ---- hit testing --------------------------------------------------------

    int FileBrowserDialog::PlaceAt(float x, float y) const
    {
        const auto &places = model.GetPlaces();
        for (std::size_t i = 0; i < places.size(); ++i) {
            if (PlaceRowRect(static_cast<int>(i)).Contains(x, y)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int FileBrowserDialog::RowAt(float x, float y) const
    {
        const sky::ui::UIRect list = ListRect();
        if (!list.Contains(x, y)) {
            return -1;
        }
        const auto &entries = model.GetEntries();
        const int   visible = std::max(0, static_cast<int>(list.Height() / kRowH));
        const int   count   = std::min(static_cast<int>(entries.size()), visible);
        for (int i = 0; i < count; ++i) {
            if (RowRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    int FileBrowserDialog::FilterItemCount() const
    {
        return 1 + static_cast<int>(model.GetFilters().size());
    }

    int FileBrowserDialog::FilterItemAt(float x, float y) const
    {
        for (int i = 0; i < FilterItemCount(); ++i) {
            if (FilterItemRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    int FileBrowserDialog::ContextItemCount() const
    {
        return static_cast<int>(ContextItems().size());
    }

    std::vector<std::string> FileBrowserDialog::ContextItems() const
    {
        std::vector<std::string> items;
        if (WriteMode()) {
            items.emplace_back("New Folder");
        }
        if (contextMenuRow >= 0) {
            items.emplace_back("Rename");
        }
        return items;
    }

    sky::ui::UIRect FileBrowserDialog::ContextMenuRect() const
    {
        const float           height = static_cast<float>(ContextItemCount()) * kItemH;
        const sky::ui::UIRect bounds = GetBounds();
        float                 left   = contextX;
        float                 top    = contextY;
        if (top + height > bounds.bottom) {
            top = bounds.bottom - height;
        }
        if (left + 150.0f > bounds.right) {
            left = bounds.right - 150.0f;
        }
        return sky::ui::UIRect{left, top, left + 150.0f, top + height};
    }

    sky::ui::UIRect FileBrowserDialog::ContextItemRect(int index) const
    {
        const sky::ui::UIRect menu = ContextMenuRect();
        const float           top  = menu.top + static_cast<float>(index) * kItemH;
        return sky::ui::UIRect{menu.left, top, menu.right, top + kItemH};
    }

    int FileBrowserDialog::ContextItemAt(float x, float y) const
    {
        for (int i = 0; i < ContextItemCount(); ++i) {
            if (ContextItemRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    void FileBrowserDialog::BeginRename(const std::string &name)
    {
        nameEdit.SetText(name);
        nameEdit.SelectAll();
        renameActive = true;
        MarkPaintDirty();
    }

    void FileBrowserDialog::CommitRename()
    {
        if (!renameActive) {
            return;
        }
        renameActive = false;
        if (model.RenameSelected(nameEdit.GetText())) {
            SyncName();
        } else {
            nameEdit.SetText(model.GetName());
        }
        MarkPaintDirty();
    }

    int FileBrowserDialog::ButtonAt(float x, float y) const
    {
        for (int i = 0; i < 2; ++i) {
            if (ButtonRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    // ---- actions ------------------------------------------------------------

    void FileBrowserDialog::Accept()
    {
        if (!model.CanAccept()) {
            return;
        }
        FileBrowserResult result;
        result.accepted  = true;
        result.directory = model.GetMode() == FileBrowserMode::SELECT_DIRECTORY;
        result.path      = model.ResultPath();
        Close();
        if (onResult) {
            onResult(result);
        }
    }

    void FileBrowserDialog::Cancel()
    {
        FileBrowserResult result;
        result.accepted = false;
        Close();
        if (onResult) {
            onResult(result);
        }
    }

    // ---- painting -----------------------------------------------------------

    void FileBrowserDialog::PaintChrome(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = PanelRect();

        PaintBackdrop(context);
        uc::SoftShadow(context, panel, 8.0f);
        uc::RoundedField(context, panel, th.colors.panel, th.colors.border, th.metrics.panelRadius);

        const sky::ui::UIRect title{panel.left, panel.top, panel.right, panel.top + kTitleH};
        skin.DrawPanelHeader(context, title);
        const std::string &titleText = !model.GetRequest().title.empty() ? model.GetRequest().title : std::string("Browse");
        uc::Text(context, titleText, th.fonts.title,
                 sky::ui::UIRect{title.left + th.metrics.padX, title.top, title.right - th.metrics.padX, title.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, false);
    }

    void FileBrowserDialog::PaintSidebar(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th      = skin.Theme();
        const sky::ui::UIRect sidebar = SidebarRect();
        uc::Fill(context, sidebar, th.colors.section);
        const auto &places = model.GetPlaces();
        for (std::size_t i = 0; i < places.size(); ++i) {
            const sky::ui::UIRect row = PlaceRowRect(static_cast<int>(i));
            skin.DrawRow(context, row, StateFor(static_cast<int>(i), false, static_cast<int>(i) == hoverPlace));
            uc::Text(context, places[i].label, th.fonts.label, sky::ui::UIRect{row.left + th.metrics.padX, row.top, row.right, row.bottom},
                     th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    void FileBrowserDialog::PaintToolbar(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th = skin.Theme();
        const sky::ui::UIRect up = UpRect();
        skin.DrawToolItem(context, up, "^", hoverUp);

        const sky::ui::UIRect path = PathRect();
        skin.DrawField(context, path, false, false);
        uc::Text(context, model.GetLocation(), th.fonts.value,
                 sky::ui::UIRect{path.left + th.metrics.controlPad, path.top, path.right - th.metrics.controlPad, path.bottom}, th.colors.textMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);

        if (WriteMode()) {
            skin.DrawToolItem(context, NewFolderRect(), "New Folder", hoverNewFolder);
        }
    }

    void FileBrowserDialog::PaintList(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect header = ListHeaderRect();
        uc::Fill(context, header, th.colors.header);
        const float nameCol = header.left + header.Width() * 0.68f;
        uc::Text(context, "Name", th.fonts.label, sky::ui::UIRect{header.left + th.metrics.padX, header.top, nameCol, header.bottom},
                 th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        uc::Text(context, "Type", th.fonts.label, sky::ui::UIRect{nameCol, header.top, header.right - th.metrics.padX, header.bottom},
                 th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);

        const sky::ui::UIRect list = ListRect();
        uc::Fill(context, list, th.colors.window);
        uc::Border(context, list, th.colors.borderSoft);

        const auto &entries = model.GetEntries();
        const int   visible = static_cast<int>(list.Height() / kRowH);
        const int   count   = std::min(static_cast<int>(entries.size()), visible);
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect row = RowRect(i);
            skin.DrawRow(context, row, StateFor(i, i == model.GetSelected(), i == hoverRow));

            const FileBrowserEntry &entry      = entries[static_cast<std::size_t>(i)];
            const float             rowNameCol = row.left + list.Width() * 0.68f;
            uc::Text(context, entry.name, th.fonts.value, sky::ui::UIRect{row.left + th.metrics.padX, row.top, rowNameCol, row.bottom},
                     th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
            uc::Text(context, TypeLabelFor(entry), th.fonts.value, sky::ui::UIRect{rowNameCol, row.top, row.right - th.metrics.padX, row.bottom},
                     th.colors.textDisabled, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }

        if (!model.GetError().empty()) {
            uc::Text(context, model.GetError(), th.fonts.value,
                     sky::ui::UIRect{list.left + th.metrics.padX, list.bottom - 18.0f, list.right, list.bottom}, th.colors.textMuted, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    void FileBrowserDialog::PaintFooter(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect panel  = PanelRect();
        const sky::ui::UIRect footer = FooterRect();
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);

        const sky::ui::UIRect filter = FilterRect();
        skin.DrawField(context, filter, false, false);
        uc::Text(context, model.GetFilterLabel(model.GetActiveFilter()), th.fonts.value,
                 sky::ui::UIRect{filter.left + th.metrics.controlPad, filter.top, filter.right - 20.0f, filter.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, true);
        skin.DrawTriangle(context, filter.right - 12.0f, (filter.top + filter.bottom) * 0.5f, true, th.colors.textMuted);

        const sky::ui::UIRect nameField = NameFieldRect();
        skin.DrawField(context, nameField, false, false);
        const std::string &name = nameEdit.GetText();
        if (nameEdit.HasSelection() && textSystem != nullptr) {
            const std::size_t selStart = nameEdit.GetSelectionStart();
            const std::size_t selEnd   = nameEdit.GetSelectionEnd();
            const float       startX   = nameField.left + uc::TextWidth(name.substr(0, selStart), th.fonts.value, textSystem);
            const float       endX     = nameField.left + uc::TextWidth(name.substr(0, selEnd), th.fonts.value, textSystem);
            uc::Fill(context, sky::ui::UIRect{startX, nameField.top + 3.0f, endX, nameField.bottom - 3.0f}, th.colors.rowSelected);
        }
        uc::Text(context, name, th.fonts.value, nameField, th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        const float caretX = NameCaretX();
        uc::Fill(context, sky::ui::UIRect{caretX, nameField.top + 5.0f, caretX + 1.0f, nameField.bottom - 5.0f}, th.colors.text);

        const char *labels[2] = {"Open", "Cancel"};
        for (int i = 0; i < 2; ++i) {
            const sky::ui::UIRect btn     = ButtonRect(i);
            const bool            primary = (i == 0);
            const bool            enabled = !primary || model.CanAccept();
            if (primary) {
                const uint32_t bg = enabled ? (hoverButton == i ? th.colors.accentHover : th.colors.accent) : th.colors.section;
                uc::RoundedField(context, btn, bg, th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, labels[i], th.fonts.value, btn, enabled ? th.colors.textOnAccent : th.colors.textDisabled, textSystem,
                         uc::HAlign::Center, uc::VAlign::Middle, false);
            } else {
                skin.DrawToolItem(context, btn, labels[i], hoverButton == i);
            }
        }
    }

    void FileBrowserDialog::PaintFilterPopup(sky::ui::UIPaintContext &context) const
    {
        if (!filterPopupOpen) {
            return;
        }
        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect filter = FilterRect();
        const sky::ui::UIRect menu{filter.left, FilterItemRect(0).top, filter.right, filter.top};
        skin.DrawPopup(context, menu);
        const int count = FilterItemCount();
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect item        = FilterItemRect(i);
            const int             filterIndex = i - 1;
            skin.DrawPopupItem(context, item, i == hoverFilterItem, filterIndex == model.GetActiveFilter());
            uc::Text(context, model.GetFilterLabel(filterIndex), th.fonts.value,
                     sky::ui::UIRect{item.left + th.metrics.padX, item.top, item.right, item.bottom}, th.colors.text, textSystem, uc::HAlign::Left,
                     uc::VAlign::Middle, true);
        }
    }

    void FileBrowserDialog::PaintContextMenu(sky::ui::UIPaintContext &context) const
    {
        if (!contextMenuOpen) {
            return;
        }
        const UiTheme        &th    = skin.Theme();
        const auto            items = ContextItems();
        const sky::ui::UIRect menu  = ContextMenuRect();
        skin.DrawPopup(context, menu);
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const sky::ui::UIRect item = ContextItemRect(i);
            skin.DrawPopupItem(context, item, i == hoverContextItem, false);
            uc::Text(context, items[static_cast<std::size_t>(i)], th.fonts.value,
                     sky::ui::UIRect{item.left + th.metrics.padX, item.top, item.right, item.bottom}, th.colors.text, textSystem, uc::HAlign::Left,
                     uc::VAlign::Middle, false);
        }
    }

    void FileBrowserDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen) {
            return;
        }
        PaintChrome(context);
        PaintSidebar(context);
        PaintToolbar(context);
        PaintList(context);
        PaintFooter(context);
        PaintFilterPopup(context);
        PaintContextMenu(context);
    }

    // ---- input --------------------------------------------------------------

    sky::ui::UIEventResult FileBrowserDialog::HandlePointerDown(float x, float y)
    {
        if (contextMenuOpen) {
            const int item = ContextItemAt(x, y);
            if (item >= 0) {
                const std::vector<std::string> items = ContextItems();
                const std::string             &label = items[static_cast<std::size_t>(item)];
                if (label == "New Folder") {
                    if (model.CreateFolder()) {
                        BeginRename(model.GetName());
                    }
                } else if (label == "Rename") {
                    if (const FileBrowserEntry *entry = model.GetSelectedEntry()) {
                        BeginRename(entry->name);
                    }
                }
            }
            contextMenuOpen  = false;
            hoverContextItem = -1;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (renameActive && !NameFieldRect().Contains(x, y)) {
            CommitRename();
        }

        if (filterPopupOpen) {
            const int filterItem = FilterItemAt(x, y);
            if (filterItem >= 0) {
                model.SetActiveFilter(filterItem - 1);
            }
            filterPopupOpen = false;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        const int button = ButtonAt(x, y);
        if (button == 0) {
            if (renameActive) {
                CommitRename();
            }
            Accept();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (button == 1) {
            Cancel();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (FilterRect().Contains(x, y) && !model.GetFilters().empty()) {
            filterPopupOpen = true;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (UpRect().Contains(x, y)) {
            model.NavigateToParent();
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (WriteMode() && NewFolderRect().Contains(x, y)) {
            model.CreateFolder();
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        const int place = PlaceAt(x, y);
        if (place >= 0) {
            model.SetDirectory(model.GetPlaces()[static_cast<std::size_t>(place)].location);
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (NameFieldRect().Contains(x, y)) {
            const long long now         = NowMs();
            const bool      doubleClick = (lastClickRow == -2 && now - lastClickMs < 400);
            lastClickRow                = -2;
            lastClickMs                 = now;
            if (doubleClick) {
                nameEdit.SelectAll();
            } else {
                nameEdit.SetCaret(static_cast<std::size_t>(NameCaretFromX(x)));
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        const int row = RowAt(x, y);
        if (row >= 0) {
            const long long now         = NowMs();
            const bool      doubleClick = (row == lastClickRow && now - lastClickMs < 400);
            lastClickRow                = row;
            lastClickMs                 = now;

            const auto             &entries = model.GetEntries();
            const FileBrowserEntry &entry   = entries[static_cast<std::size_t>(row)];
            if (doubleClick && entry.isDirectory) {
                model.NavigateTo(entry.name);
            } else if (doubleClick) {
                model.SetSelected(row);
                nameEdit.SetText(entry.name);
                SyncName();
                Accept();
            } else {
                model.SetSelected(row);
                if (!entry.isDirectory) {
                    nameEdit.SetText(entry.name);
                    SyncName();
                }
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult FileBrowserDialog::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const bool popup = filterPopupOpen;
            hoverContextItem = contextMenuOpen ? ContextItemAt(event.x, event.y) : -1;
            hoverPlace       = PlaceAt(event.x, event.y);
            hoverRow         = popup ? -1 : RowAt(event.x, event.y);
            hoverFilterItem  = popup ? FilterItemAt(event.x, event.y) : -1;
            hoverButton      = popup ? -1 : ButtonAt(event.x, event.y);
            hoverUp          = !popup && UpRect().Contains(event.x, event.y);
            hoverNewFolder   = !popup && WriteMode() && NewFolderRect().Contains(event.x, event.y);
            hoverFilter      = !popup && FilterRect().Contains(event.x, event.y);
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::DOWN) {
            if (event.button == 1) { // right-click: context menu
                if (renameActive) {
                    CommitRename();
                }
                const int row = RowAt(event.x, event.y);
                if (row >= 0) {
                    model.SetSelected(row);
                    contextMenuRow  = row;
                    contextX        = event.x;
                    contextY        = event.y;
                    contextMenuOpen = true;
                    if (const FileBrowserEntry *entry = model.GetSelectedEntry()) {
                        nameEdit.SetText(entry->name);
                        SyncName();
                    }
                    MarkPaintDirty();
                }
                return sky::ui::UIEventResult::HANDLED;
            }
            return HandlePointerDown(event.x, event.y);
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult FileBrowserDialog::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (!isOpen || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        // Virtual-key codes: the host maps platform ScanCode -> VK before forwarding.
        constexpr uint32_t kEscape = 0x1B;
        constexpr uint32_t kReturn = 0x0D;

        if (event.keyCode == kEscape) {
            if (contextMenuOpen) {
                contextMenuOpen = false;
                MarkPaintDirty();
            } else if (renameActive) {
                renameActive = false;
                nameEdit.SetText(model.GetName());
                MarkPaintDirty();
            } else if (filterPopupOpen) {
                filterPopupOpen = false;
                MarkPaintDirty();
            } else {
                Cancel();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kReturn) {
            if (renameActive) {
                CommitRename();
            } else {
                Accept();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        const bool shift = (event.modifiers & kModShift) != 0;
        const bool ctrl  = (event.modifiers & kModCtrl) != 0;
        if (nameEdit.OnKey(event.keyCode, shift, ctrl)) {
            SyncName();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult FileBrowserDialog::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (nameEdit.OnText(event.text)) {
            SyncName();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

} // namespace sky::editor
