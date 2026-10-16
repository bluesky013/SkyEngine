//
// Created on 2026/10/06.
//

#include <editor/shell/FileBrowserDialog.h>

#include <editor/core/input/KeyModifiers.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/ScrollView.h>
#include <ui/widgets/VBox.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }

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

        bool HasArea(const sky::ui::UIRect &rect)
        {
            return rect.right > rect.left && rect.bottom > rect.top;
        }

        bool Intersects(const sky::ui::UIRect &a, const sky::ui::UIRect &b)
        {
            return HasArea(a) && HasArea(b) && a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
        }

        sky::ui::UIRect Viewport(const sky::ui::UIElement &element)
        {
            return element.GetLayout().ContentRect(element.GetBounds());
        }

        void Padding(sky::ui::UIElement &element, float horizontal, float vertical)
        {
            auto layout        = element.GetLayout();
            layout.paddingLeft = layout.paddingRight = horizontal;
            layout.paddingTop = layout.paddingBottom = vertical;
            element.SetLayout(layout);
        }

        std::unique_ptr<sky::ui::UIElement> Slot(const char *name)
        {
            auto result = std::make_unique<sky::ui::UIElement>();
            result->SetName(name);
            result->SetLayoutVolatile(false);
            return result;
        }
    } // namespace

    struct FileBrowserDialog::Layout {
        sky::ui::DockPanel                 *panel   = nullptr;
        sky::ui::UIElement                 *title   = nullptr;
        sky::ui::DockPanel                 *body    = nullptr;
        sky::ui::ScrollView                *sidebar = nullptr;
        sky::ui::VBox                      *places  = nullptr;
        std::vector<sky::ui::UIElement *>   placeRows;
        sky::ui::DockPanel                 *content    = nullptr;
        sky::ui::DockPanel                 *toolbar    = nullptr;
        sky::ui::UIElement                 *up         = nullptr;
        sky::ui::UIElement                 *path       = nullptr;
        sky::ui::UIElement                 *newFolder  = nullptr;
        sky::ui::UIElement                 *listHeader = nullptr;
        sky::ui::ScrollView                *list       = nullptr;
        sky::ui::VBox                      *entries    = nullptr;
        std::vector<sky::ui::UIElement *>   entryRows;
        sky::ui::DockPanel                 *footer  = nullptr;
        sky::ui::GridPanel                 *fields  = nullptr;
        sky::ui::GridPanel                 *buttons = nullptr;
        sky::ui::UIElement                 *filter  = nullptr;
        sky::ui::UIElement                 *name    = nullptr;
        std::array<sky::ui::UIElement *, 2> buttonItems{};
        int                                 fieldColumns  = 0;
        int                                 buttonColumns = 0;
        float                               fieldMargin   = -1.0f;
        float                               buttonMargin  = -1.0f;
        std::string                         location;
        int                                 activeFilter = -2;
    };

    FileBrowserDialog::FileBrowserDialog(sky::ui::UITextSystem *text)
        : composition(std::make_unique<Layout>()), textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        using namespace sky::ui;
        SetVisible(false);
        auto &parts   = *composition;
        parts.panel   = static_cast<DockPanel *>(AddChild(std::make_unique<DockPanel>()));
        parts.title   = parts.panel->Add(Slot("file-browser-title"), {DockSide::Top});
        parts.footer  = static_cast<DockPanel *>(parts.panel->Add(std::make_unique<DockPanel>(), {DockSide::Bottom}));
        parts.buttons = static_cast<GridPanel *>(parts.footer->Add(std::make_unique<GridPanel>(), {DockSide::Right}));
        parts.fields  = static_cast<GridPanel *>(parts.footer->Add(std::make_unique<GridPanel>()));
        parts.body    = static_cast<DockPanel *>(parts.panel->Add(std::make_unique<DockPanel>()));
        parts.sidebar = static_cast<ScrollView *>(parts.body->Add(std::make_unique<ScrollView>(), {DockSide::Left}));
        parts.sidebar->SetScrollAxes(ScrollAxes::Vertical);
        parts.places     = static_cast<VBox *>(parts.sidebar->SetContent(std::make_unique<VBox>()));
        parts.content    = static_cast<DockPanel *>(parts.body->Add(std::make_unique<DockPanel>()));
        parts.toolbar    = static_cast<DockPanel *>(parts.content->Add(std::make_unique<DockPanel>(), {DockSide::Top}));
        parts.up         = parts.toolbar->Add(Slot("file-browser-up"), {DockSide::Left});
        parts.newFolder  = parts.toolbar->Add(Slot("file-browser-new-folder"), {DockSide::Right});
        parts.path       = parts.toolbar->Add(Slot("file-browser-path"));
        parts.listHeader = parts.content->Add(Slot("file-browser-list-header"), {DockSide::Top});
        parts.list       = static_cast<ScrollView *>(parts.content->Add(std::make_unique<ScrollView>()));
        parts.list->SetScrollAxes(ScrollAxes::Vertical);
        parts.entries = static_cast<VBox *>(parts.list->SetContent(std::make_unique<VBox>()));
        UILayoutParams listLayout;
        listLayout.width.mode = UISizeMode::FILL;
        parts.places->SetLayout(listLayout);
        parts.entries->SetLayout(listLayout);
    }

    FileBrowserDialog::~FileBrowserDialog() = default;

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
        composition->sidebar->SetScroll(0.0f, 0.0f);
        composition->list->SetScroll(0.0f, 0.0f);
        SynchronizeLayout();
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

    void FileBrowserDialog::UpdateComposition()
    {
        using namespace sky::ui;
        auto        &parts     = *composition;
        const auto  &m         = M();
        const auto   available = GetArrangedSize();
        const float  margin    = std::max(0.0f, m.dialogMargin);
        const UISize size{std::min(std::max(0.0f, m.fileBrowserPanelWidth), std::max(0.0f, available.width - 2.0f * margin)),
                          std::min(std::max(0.0f, m.fileBrowserPanelHeight), std::max(0.0f, available.height - 2.0f * margin))};
        const auto  &bounds = GetBounds();
        const float  left   = bounds.left + (available.width - size.width) * 0.5f;
        const float  top    = bounds.top + (available.height - size.height) * 0.5f;
        const UIRect panel{left, top, left + size.width, top + size.height};
        const float  titleHeight = std::min(std::max(0.0f, m.titleBarHeight), size.height);
        const float  padX        = std::min(margin, size.width * 0.5f);
        const float  innerWidth  = std::max(0.0f, size.width - 2.0f * padX);
        const float  gap         = std::max(0.0f, m.itemSpacing);
        const float  frame       = std::max(0.0f, m.frameHeight);
        float        buttonWidth = std::max(0.0f, m.buttonMinWidth);
        for (const char *label : {"Open", "Cancel"}) {
            buttonWidth = std::max(buttonWidth, uc::TextWidth(label, skin.Theme().fonts.value, textSystem) + 2.0f * m.controlPad);
        }
        const bool  inlineFooter    = innerWidth >= 2.0f * std::max(0.0f, m.fieldWidth) + 2.0f * buttonWidth + 3.0f * gap;
        const int   fieldColumns    = inlineFooter || innerWidth >= 2.0f * std::max(0.0f, m.fieldWidth) ? 2 : 1;
        const int   fieldRows       = 2 / fieldColumns;
        const int   buttonColumns   = innerWidth >= 2.0f * buttonWidth + gap ? 2 : 1;
        const int   buttonRows      = 2 / buttonColumns;
        const float requestedFooter = inlineFooter ? frame : static_cast<float>(fieldRows + buttonRows) * frame + gap;
        const float footerHeight    = std::min(std::max(std::max(0.0f, m.footerHeight), requestedFooter + 2.0f * std::max(0.0f, m.padX)),
                                               std::max(0.0f, size.height - titleHeight));
        const float padY            = std::min(std::max(0.0f, m.padX), footerHeight * 0.5f);
        const float innerHeight     = std::max(0.0f, footerHeight - 2.0f * padY);
        const float buttonExtent =
            inlineFooter ? std::min(innerWidth, 2.0f * buttonWidth + gap) : std::min(innerHeight, static_cast<float>(buttonRows) * frame + gap);
        const float buttonsWidth  = inlineFooter ? buttonExtent : innerWidth;
        const float buttonsHeight = inlineFooter ? innerHeight : buttonExtent;
        const float fieldsWidth   = inlineFooter ? std::max(0.0f, innerWidth - buttonExtent) : innerWidth;
        const float fieldsHeight  = inlineFooter ? innerHeight : std::max(0.0f, innerHeight - buttonExtent);
        const float buttonMargin  = std::min({gap * 0.5f, buttonsWidth / (2.0f * buttonColumns), buttonsHeight / (2.0f * buttonRows)});
        const float fieldMargin   = std::min({gap * 0.5f, fieldsWidth / (2.0f * fieldColumns), fieldsHeight / (2.0f * fieldRows)});
        if (parts.buttonColumns != buttonColumns || parts.buttonMargin != buttonMargin) {
            parts.buttons->ClearChildren();
            parts.buttons->SetGrid(buttonRows, buttonColumns);
            for (int i = 0; i < 2; ++i)
                parts.buttonItems[i] = parts.buttons->Add(Slot(i == 0 ? "file-browser-accept" : "file-browser-cancel"),
                                                          {i / buttonColumns, i % buttonColumns, 1, 1, buttonMargin});
            parts.buttonColumns = buttonColumns;
            parts.buttonMargin  = buttonMargin;
        }
        if (parts.fieldColumns != fieldColumns || parts.fieldMargin != fieldMargin) {
            parts.fields->ClearChildren();
            parts.fields->SetGrid(fieldRows, fieldColumns);
            parts.filter       = parts.fields->Add(Slot("file-browser-filter"), {0, 0, 1, 1, fieldMargin});
            parts.name         = parts.fields->Add(Slot("file-browser-name"), {1 / fieldColumns, 1 % fieldColumns, 1, 1, fieldMargin});
            parts.fieldColumns = fieldColumns;
            parts.fieldMargin  = fieldMargin;
        }
        Padding(*parts.footer, padX, padY);
        parts.footer->SetSlot(parts.buttons, {inlineFooter ? DockSide::Right : DockSide::Bottom, buttonExtent});
        parts.panel->SetSlot(parts.title, {DockSide::Top, titleHeight});
        parts.panel->SetSlot(parts.footer, {DockSide::Bottom, footerHeight});
        const float bodyHeight   = std::max(0.0f, size.height - titleHeight - footerHeight);
        const float sidebarWidth = std::min(std::max(0.0f, m.sidebarWidth), size.width * 0.35f);
        parts.body->SetSlot(parts.sidebar, {DockSide::Left, sidebarWidth});
        const float sidebarPad     = std::min(std::max(0.0f, m.cellPadding), sidebarWidth * 0.5f);
        const float sidebarTrack   = std::min(std::max(0.0f, m.scrollBarWidth), std::max(0.0f, sidebarWidth - sidebarPad));
        auto        sidebarLayout  = parts.sidebar->GetLayout();
        sidebarLayout.paddingLeft  = sidebarPad;
        sidebarLayout.paddingRight = sidebarTrack;
        sidebarLayout.paddingTop = sidebarLayout.paddingBottom = std::min(std::max(0.0f, m.cellPadding), bodyHeight * 0.5f);
        parts.sidebar->SetLayout(sidebarLayout);
        const float contentWidth = std::max(0.0f, size.width - sidebarWidth);
        const float contentPad   = std::min(margin, contentWidth * 0.5f);
        Padding(*parts.content, contentPad, 0.0f);
        const float toolbarHeight = std::min(std::max(0.0f, m.toolbarHeight), bodyHeight);
        const float headerHeight  = std::min(std::max(0.0f, m.listHeaderHeight), std::max(0.0f, bodyHeight - toolbarHeight));
        parts.content->SetSlot(parts.toolbar, {DockSide::Top, toolbarHeight});
        parts.content->SetSlot(parts.listHeader, {DockSide::Top, headerHeight});
        const float toolbarWidth = std::max(0.0f, contentWidth - 2.0f * contentPad);
        Padding(*parts.toolbar, std::min(std::max(0.0f, m.checkboxPad), toolbarWidth * 0.5f),
                std::min(std::max(0.0f, m.hairline), toolbarHeight * 0.5f));
        const auto  toolbarInner = parts.toolbar->GetLayout().ContentSize({toolbarWidth, toolbarHeight});
        const float upWidth      = std::min(std::max(0.0f, m.iconButtonWidth), toolbarInner.width * 0.2f);
        const float folderWidth =
            WriteMode() ? std::min(std::max(0.0f, m.controlButtonWidth), std::max(0.0f, toolbarInner.width - upWidth) * 0.4f) : 0.0f;
        parts.toolbar->SetSlot(parts.up, {DockSide::Left, upWidth});
        parts.toolbar->SetSlot(parts.newFolder, {DockSide::Right, folderWidth});
        parts.newFolder->SetVisible(WriteMode());
        auto listLayout         = parts.list->GetLayout();
        listLayout.paddingRight = std::min(std::max(0.0f, m.scrollBarWidth), toolbarWidth);
        parts.list->SetLayout(listLayout);
        const auto updateRows = [&](VBox &list, std::vector<UIElement *> &rows, std::size_t count) {
            if (rows.size() != count) {
                list.ClearChildren();
                rows.clear();
                for (std::size_t i = 0; i < count; ++i)
                    rows.push_back(list.AddChild(Slot("file-browser-row")));
            }
            const float rowHeight = std::max(0.0f, m.rowHeight);
            const float rowGap    = std::min(std::max(0.0f, m.checkboxPad), rowHeight);
            list.SetSpacing(rowGap);
            for (auto *row : rows) {
                UILayoutParams layout;
                layout.width.mode = UISizeMode::FILL;
                layout.height     = {UISizeMode::FIXED, rowHeight - rowGap};
                row->SetLayout(layout);
            }
        };
        updateRows(*parts.places, parts.placeRows, model.GetPlaces().size());
        updateRows(*parts.entries, parts.entryRows, model.GetEntries().size());
        if (parts.location != model.GetLocation() || parts.activeFilter != model.GetActiveFilter()) {
            parts.list->SetScroll(0.0f, 0.0f);
            parts.location     = model.GetLocation();
            parts.activeFilter = model.GetActiveFilter();
            lastClickRow       = -1;
        }
        parts.panel->ArrangeIfNeeded(panel, size);
    }

    void FileBrowserDialog::SynchronizeLayout()
    {
        if (layoutBusy)
            return;
        layoutBusy = true;
        SynchronizeDefaultUiTheme(*this, themeRevision);
        UpdateComposition();
        layoutBusy = false;
    }

    void FileBrowserDialog::ArrangeChildren(const sky::ui::UIRect &)
    {
        SynchronizeLayout();
    }

    sky::ui::UIRect FileBrowserDialog::PanelRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->panel->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::SidebarRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->sidebar->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::PlaceRowRect(int index) const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return index >= 0 && index < static_cast<int>(composition->placeRows.size()) ? composition->placeRows[index]->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect FileBrowserDialog::ToolbarRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->toolbar->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::UpRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->up->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::PathRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->path->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::NewFolderRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return WriteMode() ? composition->newFolder->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect FileBrowserDialog::ListHeaderRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->listHeader->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::ListRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return Viewport(*composition->list);
    }

    sky::ui::UIRect FileBrowserDialog::RowRect(int index) const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return index >= 0 && index < static_cast<int>(composition->entryRows.size()) ? composition->entryRows[index]->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect FileBrowserDialog::FooterRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->footer->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::FilterRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->filter->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::FilterItemRect(int index) const
    {
        const auto filter = FilterRect();
        const int  count  = FilterItemCount();
        if (index < 0 || index >= count)
            return {};
        const float height = std::max(0.0f, std::min(M().rowHeight, (filter.top - GetBounds().top) / std::max(1, count)));
        const float top    = filter.top - static_cast<float>(count - index) * height;
        return {filter.left, top, filter.right, top + height};
    }

    sky::ui::UIRect FileBrowserDialog::NameFieldRect() const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return composition->name->GetBounds();
    }

    sky::ui::UIRect FileBrowserDialog::ButtonRect(int index) const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        return index >= 0 && index < 2 ? composition->buttonItems[index]->GetBounds() : sky::ui::UIRect{};
    }

    // ---- hit testing --------------------------------------------------------

    int FileBrowserDialog::PlaceAt(float x, float y) const
    {
        const_cast<FileBrowserDialog *>(this)->SynchronizeLayout();
        const auto viewport = Viewport(*composition->sidebar);
        if (!HasArea(viewport) || !viewport.Contains(x, y))
            return -1;
        const auto &places = model.GetPlaces();
        for (std::size_t i = 0; i < places.size(); ++i) {
            const auto row = composition->placeRows[i]->GetBounds();
            if (HasArea(row) && row.Contains(x, y)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int FileBrowserDialog::RowAt(float x, float y) const
    {
        const sky::ui::UIRect list = ListRect();
        if (!HasArea(list) || !list.Contains(x, y)) {
            return -1;
        }
        const auto &entries = model.GetEntries();
        const int   count   = static_cast<int>(entries.size());
        for (int i = 0; i < count; ++i) {
            const auto row = composition->entryRows[static_cast<std::size_t>(i)]->GetBounds();
            if (HasArea(row) && row.Contains(x, y)) {
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
            const auto item = FilterItemRect(i);
            if (HasArea(item) && item.Contains(x, y)) {
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
        const sky::ui::UIRect bounds = GetBounds();
        const float           width  = std::min(std::max(0.0f, M().popupMinWidth), GetArrangedSize().width);
        const float           height = std::min(std::max(0.0f, static_cast<float>(ContextItemCount()) * M().rowHeight), GetArrangedSize().height);
        const float           left   = std::clamp(contextX, bounds.left, std::max(bounds.left, bounds.right - width));
        const float           top    = std::clamp(contextY, bounds.top, std::max(bounds.top, bounds.bottom - height));
        return {left, top, left + width, top + height};
    }

    sky::ui::UIRect FileBrowserDialog::ContextItemRect(int index) const
    {
        const sky::ui::UIRect menu = ContextMenuRect();
        if (index < 0 || index >= ContextItemCount())
            return {};
        const float height = menu.Height() / std::max(1, ContextItemCount());
        const float top    = menu.top + static_cast<float>(index) * height;
        return {menu.left, top, menu.right, top + height};
    }

    int FileBrowserDialog::ContextItemAt(float x, float y) const
    {
        for (int i = 0; i < ContextItemCount(); ++i) {
            const auto item = ContextItemRect(i);
            if (HasArea(item) && item.Contains(x, y)) {
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
            const auto button = ButtonRect(i);
            if (HasArea(button) && button.Contains(x, y)) {
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
        result.accepted     = true;
        result.directory    = model.GetMode() == FileBrowserMode::SELECT_DIRECTORY;
        result.path         = model.ResultPath();
        const auto callback = onResult;
        Close();
        if (callback) {
            callback(result);
        }
    }

    void FileBrowserDialog::Cancel()
    {
        FileBrowserResult result;
        result.accepted     = false;
        const auto callback = onResult;
        Close();
        if (callback) {
            callback(result);
        }
    }

    // ---- painting -----------------------------------------------------------

    void FileBrowserDialog::PaintChrome(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = PanelRect();

        PaintBackdrop(context);
        uc::SoftShadow(context, panel, M().itemSpacing);
        uc::RoundedField(context, panel, th.colors.panel, th.colors.border, th.metrics.panelRadius);

        const sky::ui::UIRect title = composition->title->GetBounds();
        context.PushClip(title);
        skin.DrawPanelHeader(context, title);
        const std::string &titleText = !model.GetRequest().title.empty() ? model.GetRequest().title : std::string("Browse");
        uc::Text(context, titleText, th.fonts.title,
                 sky::ui::UIRect{title.left + th.metrics.padX, title.top, title.right - th.metrics.padX, title.bottom}, th.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, false);
        context.PopClip();
    }

    void FileBrowserDialog::PaintSidebar(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th      = skin.Theme();
        const sky::ui::UIRect sidebar = SidebarRect();
        uc::Fill(context, sidebar, th.colors.section);
        const auto viewport = Viewport(*composition->sidebar);
        context.PushClip(viewport);
        const auto &places = model.GetPlaces();
        for (std::size_t i = 0; i < places.size(); ++i) {
            const sky::ui::UIRect row = PlaceRowRect(static_cast<int>(i));
            if (!Intersects(row, viewport))
                continue;
            skin.DrawRow(context, row, StateFor(static_cast<int>(i), false, static_cast<int>(i) == hoverPlace));
            uc::Text(context, places[i].label, th.fonts.label, sky::ui::UIRect{row.left + th.metrics.padX, row.top, row.right, row.bottom},
                     th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
        context.PopClip();
        auto track  = viewport;
        track.left  = viewport.right;
        track.right = sidebar.right;
        skin.DrawScrollbar(context, track, composition->sidebar->GetContentHeight(), composition->sidebar->GetScrollY());
    }

    void FileBrowserDialog::PaintToolbar(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th = skin.Theme();
        const sky::ui::UIRect up = UpRect();
        context.PushClip(up);
        skin.DrawToolItem(context, up, "^", hoverUp);
        context.PopClip();

        const sky::ui::UIRect path = PathRect();
        context.PushClip(path);
        skin.DrawField(context, path, false, false);
        uc::Text(context, model.GetLocation(), th.fonts.value,
                 sky::ui::UIRect{path.left + th.metrics.controlPad, path.top, path.right - th.metrics.controlPad, path.bottom}, th.colors.textMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        context.PopClip();

        if (WriteMode()) {
            context.PushClip(NewFolderRect());
            skin.DrawToolItem(context, NewFolderRect(), "New Folder", hoverNewFolder);
            context.PopClip();
        }
    }

    void FileBrowserDialog::PaintList(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect header = ListHeaderRect();
        context.PushClip(header);
        uc::Fill(context, header, th.colors.header);
        const float nameCol = header.left + header.Width() * 0.68f;
        uc::Text(context, "Name", th.fonts.label, sky::ui::UIRect{header.left + th.metrics.padX, header.top, nameCol, header.bottom},
                 th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        uc::Text(context, "Type", th.fonts.label, sky::ui::UIRect{nameCol, header.top, header.right - th.metrics.padX, header.bottom},
                 th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        context.PopClip();

        const sky::ui::UIRect list = ListRect();
        uc::Fill(context, list, th.colors.window);
        uc::Border(context, list, th.colors.borderSoft);

        const auto &entries = model.GetEntries();
        const int   count   = static_cast<int>(entries.size());
        context.PushClip(list);
        for (int i = 0; i < count; ++i) {
            const sky::ui::UIRect row = RowRect(i);
            if (!Intersects(row, list))
                continue;
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
                     sky::ui::UIRect{list.left + th.metrics.padX, list.bottom - (M().indentSmall + M().checkboxPad), list.right, list.bottom},
                     th.colors.textMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
        context.PopClip();
        auto track  = list;
        track.left  = list.right;
        track.right = composition->list->GetBounds().right;
        skin.DrawScrollbar(context, track, composition->list->GetContentHeight(), composition->list->GetScrollY());
    }

    void FileBrowserDialog::PaintFooter(sky::ui::UIPaintContext &context) const
    {
        const UiTheme        &th     = skin.Theme();
        const sky::ui::UIRect panel  = PanelRect();
        const sky::ui::UIRect footer = FooterRect();
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);

        const sky::ui::UIRect filter = FilterRect();
        context.PushClip(filter);
        skin.DrawField(context, filter, false, false);
        uc::Text(context, model.GetFilterLabel(model.GetActiveFilter()), th.fonts.value,
                 sky::ui::UIRect{filter.left + th.metrics.controlPad, filter.top, filter.right - M().padX * 2.0f, filter.bottom}, th.colors.text,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        skin.DrawTriangle(context, filter.right - M().cellPadding * 2.0f, (filter.top + filter.bottom) * 0.5f, true, th.colors.textMuted);
        context.PopClip();

        const sky::ui::UIRect nameField = NameFieldRect();
        context.PushClip(nameField);
        skin.DrawField(context, nameField, false, false);
        const std::string &name = nameEdit.GetText();
        if (nameEdit.HasSelection() && textSystem != nullptr) {
            const std::size_t selStart = nameEdit.GetSelectionStart();
            const std::size_t selEnd   = nameEdit.GetSelectionEnd();
            const float       startX   = nameField.left + uc::TextWidth(name.substr(0, selStart), th.fonts.value, textSystem);
            const float       endX     = nameField.left + uc::TextWidth(name.substr(0, selEnd), th.fonts.value, textSystem);
            uc::Fill(context, sky::ui::UIRect{startX, nameField.top + M().hairline, endX, nameField.bottom - M().hairline}, th.colors.rowSelected);
        }
        uc::Text(context, name, th.fonts.value, nameField, th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        const float caretX = NameCaretX();
        uc::Fill(context, sky::ui::UIRect{caretX, nameField.top + M().cellPadding, caretX + M().checkboxPad, nameField.bottom - M().cellPadding},
                 th.colors.text);
        context.PopClip();

        const char *labels[2] = {"Open", "Cancel"};
        for (int i = 0; i < 2; ++i) {
            const sky::ui::UIRect btn = ButtonRect(i);
            context.PushClip(btn);
            const bool primary = (i == 0);
            const bool enabled = !primary || model.CanAccept();
            if (primary) {
                const uint32_t bg = enabled ? (hoverButton == i ? th.colors.accentHover : th.colors.accent) : th.colors.section;
                uc::RoundedField(context, btn, bg, th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, labels[i], th.fonts.value, btn, enabled ? th.colors.textOnAccent : th.colors.textDisabled, textSystem,
                         uc::HAlign::Center, uc::VAlign::Middle, false);
            } else {
                skin.DrawToolItem(context, btn, labels[i], hoverButton == i);
            }
            context.PopClip();
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
        // Layout is prepared by Arrange, Open, input or public geometry queries;
        // painting only consumes the same arranged slots and never rebuilds them.
        const bool wasBusy = layoutBusy;
        layoutBusy         = true;
        PaintChrome(context);
        context.PushClip(composition->panel->GetBounds());
        PaintSidebar(context);
        PaintToolbar(context);
        PaintList(context);
        PaintFooter(context);
        context.PopClip();
        PaintFilterPopup(context);
        PaintContextMenu(context);
        layoutBusy = wasBusy;
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

        if (HasArea(FilterRect()) && FilterRect().Contains(x, y) && !model.GetFilters().empty()) {
            filterPopupOpen = true;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (HasArea(UpRect()) && UpRect().Contains(x, y)) {
            model.NavigateToParent();
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (WriteMode() && HasArea(NewFolderRect()) && NewFolderRect().Contains(x, y)) {
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

        if (HasArea(NameFieldRect()) && NameFieldRect().Contains(x, y)) {
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
                return sky::ui::UIEventResult::HANDLED;
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
        SynchronizeLayout();
        struct SynchronizeOnReturn {
            sky::ui::UIElement::InputReference self;
            ~SynchronizeOnReturn()
            {
                if (auto *alive = self.GetAlive())
                    static_cast<FileBrowserDialog *>(alive)->SynchronizeLayout();
            }
        } synchronize{GetInputReference()};

        if (event.action == sky::ui::UIPointerAction::WHEEL) {
            if (!filterPopupOpen && !contextMenuOpen) {
                auto mapped       = event;
                mapped.wheelDelta = -event.wheelDelta * skin.Theme().scale;
                if (HasArea(SidebarRect()) && SidebarRect().Contains(event.x, event.y))
                    composition->sidebar->OnPointerEvent(mapped);
                else if (HasArea(ListRect()) && ListRect().Contains(event.x, event.y))
                    composition->list->OnPointerEvent(mapped);
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
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
