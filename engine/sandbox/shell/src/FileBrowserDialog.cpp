//
// Created on 2026/10/06.
//

#include <editor/shell/FileBrowserDialog.h>

#include <editor/shell/UiDraw.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        constexpr float    kPanelW     = 820.0f;
        constexpr float    kPanelH     = 560.0f;
        constexpr float    kMargin     = 12.0f;
        constexpr float    kTitleH     = 36.0f;
        constexpr float    kSidebarW   = 170.0f;
        constexpr float    kToolbarH   = 30.0f;
        constexpr float    kHeaderRowH = 22.0f;
        constexpr float    kRowH       = 24.0f;
        constexpr float    kFooterH    = 46.0f;
        constexpr float    kBtnW       = 88.0f;
        constexpr float    kBtnH       = 26.0f;
        constexpr float    kItemH      = 22.0f;
        constexpr uint32_t kTitleSize  = 15;
        constexpr uint32_t kTextSize   = 13;

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
    } // namespace

    FileBrowserDialog::FileBrowserDialog(sky::ui::UITextSystem *text) : textSystem(text)
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
        // Caret at the end, nothing selected; double-click the field to select all.
        nameEdit.SetText(model.GetName());
        isOpen = true;
        SetVisible(true);
        MarkPaintDirty();
    }

    void FileBrowserDialog::Close()
    {
        isOpen          = false;
        filterPopupOpen = false;
        contextMenuOpen = false;
        renameActive    = false;
        SetVisible(false);
        MarkPaintDirty();
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
            const float w = uc::TextWidth(name.substr(i, 1), kTextSize, textSystem);
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
        // Text is drawn at rect.left with no padding; keep the caret aligned.
        return field.left + uc::TextWidth(name.substr(0, caret), kTextSize, textSystem);
    }

    void FileBrowserDialog::SyncName()
    {
        model.SetName(nameEdit.GetText());
        MarkPaintDirty();
    }

    // ---- layout -------------------------------------------------------------

    sky::ui::UIRect FileBrowserDialog::PanelRect() const
    {
        const sky::ui::UIRect bounds = GetBounds();
        const float           w      = std::min(kPanelW, std::max(360.0f, bounds.Width() - 60.0f));
        const float           h      = std::min(kPanelH, std::max(260.0f, bounds.Height() - 60.0f));
        const float           left   = bounds.left + (bounds.Width() - w) * 0.5f;
        const float           top    = bounds.top + (bounds.Height() - h) * 0.5f;
        return sky::ui::UIRect{left, top, left + w, top + h};
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
        const sky::ui::UIRect bounds = GetBounds();
        const sky::ui::UIRect panel  = PanelRect();

        uc::Fill(context, bounds, uc::RGB(0, 0, 0, 170));
        uc::SoftShadow(context, panel, 8.0f);
        uc::RoundedField(context, panel, uc::color::Panel, uc::color::Border, 6.0f);

        const sky::ui::UIRect title{panel.left, panel.top, panel.right, panel.top + kTitleH};
        uc::Fill(context, title, uc::color::Header);
        const std::string &titleText = !model.GetRequest().title.empty() ? model.GetRequest().title : std::string("Browse");
        uc::Text(context, titleText, kTitleSize, sky::ui::UIRect{title.left + kMargin, title.top, title.right - kMargin, title.bottom},
                 uc::color::Text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        uc::HLine(context, panel.left, panel.right, panel.top + kTitleH, uc::color::Border);
    }

    void FileBrowserDialog::PaintSidebar(sky::ui::UIPaintContext &context) const
    {
        const sky::ui::UIRect sidebar = SidebarRect();
        uc::Fill(context, sidebar, uc::color::Section);
        const auto &places = model.GetPlaces();
        for (std::size_t i = 0; i < places.size(); ++i) {
            const sky::ui::UIRect row = PlaceRowRect(static_cast<int>(i));
            if (static_cast<int>(i) == hoverPlace) {
                uc::Fill(context, row, uc::color::RowHover);
            }
            uc::Text(context, places[i].label, kTextSize, row, uc::color::Text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    void FileBrowserDialog::PaintToolbar(sky::ui::UIPaintContext &context) const
    {
        const sky::ui::UIRect up = UpRect();
        uc::RoundedField(context, up, hoverUp ? uc::color::FieldHover : uc::color::Section, uc::color::Border, 4.0f);
        uc::Text(context, "^", kTextSize, up, uc::color::Text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);

        const sky::ui::UIRect path = PathRect();
        uc::Field(context, path, uc::color::Field, uc::color::BorderSoft);
        uc::Text(context, model.GetLocation(), kTextSize, path, uc::color::TextMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);

        if (WriteMode()) {
            const sky::ui::UIRect newFolder = NewFolderRect();
            uc::RoundedField(context, newFolder, hoverNewFolder ? uc::color::FieldHover : uc::color::Section, uc::color::Border, 4.0f);
            uc::Text(context, "New Folder", kTextSize, newFolder, uc::color::Text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
        }
    }

    void FileBrowserDialog::PaintList(sky::ui::UIPaintContext &context) const
    {
        const sky::ui::UIRect header = ListHeaderRect();
        uc::Fill(context, header, uc::color::Header);
        const float nameCol = header.left + header.Width() * 0.68f;
        uc::Text(context, "Name", kTextSize, sky::ui::UIRect{header.left + 8.0f, header.top, nameCol, header.bottom}, uc::color::TextMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        uc::Text(context, "Type", kTextSize, sky::ui::UIRect{nameCol, header.top, header.right - 8.0f, header.bottom}, uc::color::TextMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);

        const sky::ui::UIRect list = ListRect();
        uc::Fill(context, list, uc::color::Window);
        uc::Border(context, list, uc::color::BorderSoft);

        const auto &entries = model.GetEntries();
        const int   visible = static_cast<int>(list.Height() / kRowH);
        const int   count   = std::min(static_cast<int>(entries.size()), visible);
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect row = RowRect(i);
            uint32_t              bg  = (i % 2 == 0) ? uc::color::RowEven : uc::color::RowOdd;
            if (i == model.GetSelected()) {
                bg = uc::color::RowSelected;
            } else if (i == hoverRow) {
                bg = uc::color::RowHover;
            }
            uc::Fill(context, row, bg);

            const FileBrowserEntry &entry      = entries[static_cast<std::size_t>(i)];
            const float             rowNameCol = row.left + list.Width() * 0.68f;
            uc::Text(context, entry.name, kTextSize, sky::ui::UIRect{row.left + 8.0f, row.top, rowNameCol, row.bottom}, uc::color::Text, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
            uc::Text(context, TypeLabelFor(entry), kTextSize, sky::ui::UIRect{rowNameCol, row.top, row.right - 8.0f, row.bottom},
                     uc::color::TextDisabled, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }

        if (!model.GetError().empty()) {
            uc::Text(context, model.GetError(), kTextSize, sky::ui::UIRect{list.left + 8.0f, list.bottom - 18.0f, list.right, list.bottom},
                     uc::color::TextMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    void FileBrowserDialog::PaintFooter(sky::ui::UIPaintContext &context) const
    {
        const sky::ui::UIRect panel  = PanelRect();
        const sky::ui::UIRect footer = FooterRect();
        uc::Fill(context, footer, uc::color::Section);
        uc::HLine(context, panel.left, panel.right, footer.top, uc::color::Border);

        const sky::ui::UIRect filter = FilterRect();
        uc::Field(context, filter, hoverFilter ? uc::color::FieldHover : uc::color::Field, uc::color::BorderSoft);
        uc::Text(context, model.GetFilterLabel(model.GetActiveFilter()), kTextSize,
                 sky::ui::UIRect{filter.left + 8.0f, filter.top, filter.right - 20.0f, filter.bottom}, uc::color::Text, textSystem, uc::HAlign::Left,
                 uc::VAlign::Middle, true);
        uc::Text(context, "v", kTextSize, sky::ui::UIRect{filter.right - 18.0f, filter.top, filter.right - 4.0f, filter.bottom}, uc::color::TextMuted,
                 textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);

        const sky::ui::UIRect nameField = NameFieldRect();
        uc::Field(context, nameField, uc::color::Field, uc::color::BorderSoft);
        const std::string &name = nameEdit.GetText();
        if (nameEdit.HasSelection() && textSystem != nullptr) {
            const std::size_t selStart = nameEdit.GetSelectionStart();
            const std::size_t selEnd   = nameEdit.GetSelectionEnd();
            const float       startX   = nameField.left + uc::TextWidth(name.substr(0, selStart), kTextSize, textSystem);
            const float       endX     = nameField.left + uc::TextWidth(name.substr(0, selEnd), kTextSize, textSystem);
            uc::Fill(context, sky::ui::UIRect{startX, nameField.top + 3.0f, endX, nameField.bottom - 3.0f}, uc::color::RowSelected);
        }
        uc::Text(context, name, kTextSize, nameField, uc::color::Text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        const float caretX = NameCaretX();
        uc::Fill(context, sky::ui::UIRect{caretX, nameField.top + 5.0f, caretX + 1.0f, nameField.bottom - 5.0f}, uc::color::Text);

        const char *labels[2] = {"Open", "Cancel"};
        for (int i = 0; i < 2; ++i) {
            const sky::ui::UIRect btn     = ButtonRect(i);
            const bool            primary = (i == 0);
            const bool            enabled = !primary || model.CanAccept();
            uint32_t              bg      = primary ? (enabled ? uc::color::Accent : uc::color::Section) : uc::color::FieldHover;
            if (primary && enabled && hoverButton == i) {
                bg = uc::color::AccentHover;
            } else if (!primary && hoverButton == i) {
                bg = uc::color::RowHover;
            }
            uc::RoundedField(context, btn, bg, uc::color::Border, 4.0f);
            uc::Text(context, labels[i], kTextSize, btn, enabled ? uc::color::White : uc::color::TextDisabled, textSystem, uc::HAlign::Center,
                     uc::VAlign::Middle, false);
        }
    }

    void FileBrowserDialog::PaintFilterPopup(sky::ui::UIPaintContext &context) const
    {
        if (!filterPopupOpen) {
            return;
        }
        const sky::ui::UIRect filter = FilterRect();
        const int             count  = FilterItemCount();
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect item        = FilterItemRect(i);
            const int             filterIndex = i - 1;
            if (i == hoverFilterItem) {
                uc::Fill(context, item, uc::color::RowHover);
            } else if (filterIndex == model.GetActiveFilter()) {
                uc::Fill(context, item, uc::color::RowSelected);
            } else {
                uc::Fill(context, item, uc::color::Panel);
            }
            uc::Text(context, model.GetFilterLabel(filterIndex), kTextSize, item, uc::color::Text, textSystem, uc::HAlign::Left, uc::VAlign::Middle,
                     true);
        }
        uc::Border(context, sky::ui::UIRect{filter.left, FilterItemRect(0).top, filter.right, filter.top}, uc::color::Border);
    }

    void FileBrowserDialog::PaintContextMenu(sky::ui::UIPaintContext &context) const
    {
        if (!contextMenuOpen) {
            return;
        }
        const auto            items = ContextItems();
        const sky::ui::UIRect menu  = ContextMenuRect();
        uc::SoftShadow(context, menu, 6.0f);
        uc::Fill(context, menu, uc::color::Panel);
        uc::Border(context, menu, uc::color::Border);
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const sky::ui::UIRect item = ContextItemRect(i);
            if (i == hoverContextItem) {
                uc::Fill(context, item, uc::color::RowHover);
            }
            uc::Text(context, items[static_cast<std::size_t>(i)], kTextSize,
                     sky::ui::UIRect{item.left + 10.0f, item.top, item.right - 6.0f, item.bottom}, uc::color::Text, textSystem, uc::HAlign::Left,
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
        // KeyModFlags bits (framework/window/IWindowEvent.h).
        constexpr uint32_t kModShift = 0x0003;
        constexpr uint32_t kModCtrl  = 0x00C0;

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
        // TextEditState filters control characters (Backspace/Enter/Tab/Esc).
        if (nameEdit.OnText(event.text)) {
            SyncName();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

} // namespace sky::editor
