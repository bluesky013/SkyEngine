//
// Created on 2026/10/08.
//

#include <editor/shell/panels/AssetBrowserPanel.h>

#include <editor/core/asset/AssetThumbnailProvider.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <vector>

namespace sky::editor {

    namespace {
        namespace uc = uidraw;

        constexpr float    kRowHeight    = 20.0f;
        constexpr float    kCrumbHeight  = 22.0f;
        constexpr float    kDetailHeight = 260.0f; // details-pane max height (shares the panel below this)
        constexpr uint32_t kTextSize     = 12;

        constexpr float kTileWidth  = 84.0f;
        constexpr float kTileHeight = 84.0f;
        constexpr float kTileGap    = 8.0f;
        constexpr float kIconSize   = 52.0f;

        int TileColumns(const sky::ui::UIRect &r)
        {
            const float inner = r.right - r.left - kTileGap * 2.0f;
            const int   cols  = static_cast<int>((inner + kTileGap) / (kTileWidth + kTileGap));
            return cols < 1 ? 1 : cols;
        }

        sky::ui::UIRect TileRect(const sky::ui::UIRect &r, int index, int cols)
        {
            const int   row = index / cols;
            const int   col = index % cols;
            const float x   = r.left + kTileGap + static_cast<float>(col) * (kTileWidth + kTileGap);
            const float y   = r.top + kTileGap + static_cast<float>(row) * (kTileHeight + kTileGap);
            return {x, y, x + kTileWidth, y + kTileHeight};
        }

        int TileAt(const sky::ui::UIRect &r, float x, float y, int cols, int count)
        {
            if (x < r.left || x > r.right || y < r.top || y > r.bottom) {
                return -1;
            }
            const int col = static_cast<int>((x - r.left - kTileGap) / (kTileWidth + kTileGap));
            const int row = static_cast<int>((y - r.top - kTileGap) / (kTileHeight + kTileGap));
            if (col < 0 || col >= cols || row < 0) {
                return -1;
            }
            const int index = row * cols + col;
            return (index >= 0 && index < count) ? index : -1;
        }

        const char *CookStateLabel(AssetCookState state)
        {
            switch (state) {
            case AssetCookState::Cooking: return "cooking";
            case AssetCookState::Ready: return "ready";
            case AssetCookState::Failed: return "failed";
            default: return "not-cooked";
            }
        }

        uint32_t CookStateColor(AssetCookState state)
        {
            switch (state) {
            case AssetCookState::Cooking: return uc::color::TextMuted;
            case AssetCookState::Ready: return uc::color::AccentSoft;
            case AssetCookState::Failed: return uc::RGB(0xE0, 0x5A, 0x5A);
            default: return uc::color::TextDisabled;
            }
        }

        // Type-derived icon tint (fallback when no thumbnail provider has an image).
        uint32_t TypeIconColor(const std::string &type)
        {
            static const uint32_t palette[] = {
                uc::RGB(0x5A, 0x9B, 0xD5), uc::RGB(0x7F, 0xC2, 0x7F), uc::RGB(0xD6, 0xA6, 0x5E),
                uc::RGB(0xC2, 0x7F, 0xB0), uc::RGB(0x8F, 0x7F, 0xD0), uc::RGB(0x63, 0xB6, 0xB0),
            };
            uint32_t hash = 2166136261u;
            for (const char c : type) {
                hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
            }
            return palette[hash % (sizeof(palette) / sizeof(palette[0]))];
        }
    } // namespace

    AssetBrowserPanel::AssetBrowserPanel(std::string inTitle) : title(std::move(inTitle))
    {
        SetFocusable(true);
        SetClipsChildren(true);
    }

    void AssetBrowserPanel::FlattenTree(const EditorAssetFolder &node, int depth, bool isRoot)
    {
        if (isRoot) {
            for (const auto &child : node.folders) {
                FlattenTree(child, depth, false);
            }
            return;
        }

        const bool open = expanded.find(node.path) != expanded.end();
        treeCache.push_back(TreeRow{node.path, node.name, depth, !node.folders.empty(), open});
        if (open) {
            for (const auto &child : node.folders) {
                FlattenTree(child, depth + 1, false);
            }
        }
    }

    void AssetBrowserPanel::RebuildTree()
    {
        treeCache.clear();
        if (cachedTree != nullptr) {
            FlattenTree(*cachedTree, 0, true);
        }
    }

    void AssetBrowserPanel::ExpandAncestors(const std::string &path)
    {
        std::string current = path;
        while (!current.empty()) {
            expanded.insert(current);
            const auto slash = current.rfind('/');
            current          = slash == std::string::npos ? std::string{} : current.substr(0, slash);
        }
    }

    AssetBrowserPanel::Layout AssetBrowserPanel::ComputeLayout() const
    {
        Layout                 layout;
        const sky::ui::UIRect &content = GetBounds();
        layout.content                 = content;
        layout.rowHeight               = kRowHeight;

        const float mid = content.left + (content.right - content.left) * 0.42f;

        // Details pane shares the panel height (the bottom dock is short); clamp so the reflected
        // settings form always keeps usable space above/below.
        const float contentHeight = content.bottom - content.top;
        const float detailHeight  = std::clamp(contentHeight * 0.6f, 120.0f, kDetailHeight);
        const float listBottom    = content.bottom - detailHeight;

        layout.tree   = {content.left, content.top + kCrumbHeight, mid - 2.0f, listBottom};
        layout.items  = {mid + 2.0f, content.top + kCrumbHeight, content.right, listBottom};
        layout.detail = {content.left, listBottom, content.right, content.bottom};
        return layout;
    }

    void AssetBrowserPanel::RefreshView()
    {
        if (catalog == nullptr) {
            return;
        }
        cachedTree = std::make_unique<EditorAssetFolder>(catalog->GetTree());
        rootsCache = cachedTree->folders;
        if (currentPath.empty() && !rootsCache.empty()) {
            currentPath = rootsCache.front().path;
        }
        for (const auto &root : rootsCache) {
            expanded.insert(root.path); // mounts start expanded so children are discoverable
        }
        ExpandAncestors(currentPath);
        RebuildTree();
        folder       = catalog->ListFolder(currentPath);
        seenRevision = catalog->GetStateRevision();
        ReloadDetail();
        viewInit = true;
    }

    void AssetBrowserPanel::Navigate(const std::string &path)
    {
        currentPath = path;
        ExpandAncestors(path);
        RebuildTree();
        if (catalog != nullptr) {
            folder = catalog->ListFolder(currentPath);
        }
        MarkPaintDirty();
    }

    void AssetBrowserPanel::SelectItem(const EditorAssetItem &item)
    {
        selected = item.uuid;
        if (selection != nullptr) {
            selection->SetSelection({SelectionItem{SelectionType::ASSET, item.uuid}});
        }
        ReloadDetail();
    }

    void AssetBrowserPanel::ReloadDetail()
    {
        detailTargets.clear();
        if (catalog == nullptr) {
            detailTarget.clear();
            return;
        }
        detailTargets = catalog->GetTargetInfos(selected);

        const bool targetValid = std::any_of(detailTargets.begin(), detailTargets.end(),
                                             [this](const EditorAssetTargetInfo &info) { return info.target == detailTarget; });
        if (!targetValid) {
            detailTarget = detailTargets.empty() ? std::string{} : detailTargets.front().target;
        }
    }

    void AssetBrowserPanel::BeginRename()
    {
        for (const auto &item : folder.items) {
            if (item.uuid == selected) {
                editing     = true;
                editingUuid = item.uuid;
                edit.SetText(item.name);
                edit.SelectAll();
                MarkPaintDirty();
                return;
            }
        }
    }

    void AssetBrowserPanel::CommitEdit()
    {
        const std::string name = edit.GetText();
        editing                = false;
        if (!name.empty() && renameHandler) {
            for (const auto &item : folder.items) {
                if (item.uuid != editingUuid) {
                    continue;
                }
                if (name != item.name) {
                    const auto        slash  = item.path.rfind('/');
                    const std::string parent = slash == std::string::npos ? std::string{} : item.path.substr(0, slash + 1);
                    renameHandler(item.path, parent + name);
                }
                break;
            }
        }
        MarkPaintDirty();
    }

    void AssetBrowserPanel::CancelEdit()
    {
        editing = false;
        MarkPaintDirty();
    }

    sky::ui::UIEventResult AssetBrowserPanel::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        constexpr uint32_t kEscape = 0x1B;
        constexpr uint32_t kReturn = 0x0D;
        constexpr uint32_t kF2     = 0x71;

        if (editing) {
            if (event.keyCode == kReturn) {
                CommitEdit();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.keyCode == kEscape) {
                CancelEdit();
                return sky::ui::UIEventResult::HANDLED;
            }
            const bool shift = (event.modifiers & kModShift) != 0;
            const bool ctrl  = (event.modifiers & kModCtrl) != 0;
            if (edit.OnKey(event.keyCode, shift, ctrl)) {
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.keyCode == kF2) {
            BeginRename();
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult AssetBrowserPanel::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (editing && edit.OnText(event.text)) {
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    void AssetBrowserPanel::OpenStreamMenu(float x, float y, const EditorAssetItem &item)
    {
        menuEntries = {
            {"asset.open", "Open", true},
            {"asset.cook", "Cook", true},
            {"asset.copyReference", "Copy Reference", true},
            {"asset.showInExplorer", "Show in Explorer", true},
            {"asset.findReferences", "Find References", true},
            {"asset.rename", "Rename", item.writable},
            {"asset.duplicate", "Duplicate", item.writable},
            {"asset.move", "Move", item.writable},
            {"asset.delete", "Delete", item.writable},
        };

        const float width  = 170.0f;
        const float height = static_cast<float>(menuEntries.size()) * kRowHeight + 6.0f;
        menuRect           = {x, y, x + width, y + height};
        menuOpen           = true;
        hoverMenuIndex     = -1;
    }

    int AssetBrowserPanel::MenuIndexAt(float x, float y) const
    {
        if (!menuOpen || x < menuRect.left || x > menuRect.right || y < menuRect.top || y > menuRect.bottom) {
            return -1;
        }
        const int index = static_cast<int>((y - menuRect.top - 3.0f) / kRowHeight);
        if (index < 0 || index >= static_cast<int>(menuEntries.size()) || !menuEntries[index].enabled) {
            return -1;
        }
        return index;
    }

    bool AssetBrowserPanel::HandleMenuClick(float x, float y)
    {
        if (!menuOpen) {
            return false;
        }
        const int index = MenuIndexAt(x, y);
        if (index >= 0 && actionHandler) {
            actionHandler(menuEntries[index].id);
        }
        menuOpen       = false;
        hoverMenuIndex = -1;
        MarkPaintDirty();
        return true;
    }

    void AssetBrowserPanel::OnPaint(sky::ui::UIPaintContext &context)
    {
        UiSkin     skin(GetDefaultUiTheme(), textSystem);
        const auto content = skin.DrawPanel(context, GetBounds(), title, titleVisible);

        if (catalog == nullptr) {
            uc::Text(context, "(asset catalog unavailable)", kTextSize,
                     {content.left + 8.0f, content.top, content.right - 8.0f, content.top + kRowHeight}, uc::color::TextMuted, textSystem);
            return;
        }

        // Refresh when the catalog revision advanced (or on first paint).
        if (!viewInit || seenRevision != catalog->GetStateRevision()) {
            RefreshView();
        }

        const Layout layout = ComputeLayout();
        (void)layout;

        // Breadcrumb.
        {
            const sky::ui::UIRect crumb{content.left, content.top, content.right, content.top + kCrumbHeight};
            uc::Fill(context, crumb, uc::color::Section);
            uc::Text(context, currentPath.empty() ? "/" : currentPath, kTextSize, {crumb.left + 6.0f, crumb.top, crumb.right - 6.0f, crumb.bottom},
                     uc::color::Text, textSystem);
        }

        // Folder tree (left): collapsible virtual-path hierarchy.
        {
            const auto &tree = layout.tree;
            uc::Fill(context, tree, uc::color::Panel);
            float y = tree.top + 2.0f;
            for (const auto &row : treeCache) {
                if (y + kRowHeight > tree.bottom) {
                    break;
                }
                const sky::ui::UIRect rowRect{tree.left + 2.0f, y, tree.right - 2.0f, y + kRowHeight};
                if (row.path == currentPath) {
                    uc::Fill(context, rowRect, uc::color::RowSelected);
                }
                const float       indent = tree.left + 6.0f + static_cast<float>(row.depth) * 12.0f;
                const std::string arrow  = row.hasChildren ? (row.expanded ? "v " : "> ") : "  ";
                uc::Text(context, arrow + row.name, kTextSize, {indent, y, tree.right - 4.0f, y + kRowHeight}, uc::color::Text, textSystem);
                y += kRowHeight;
            }
        }

        // Item view (right): thumbnail icon tiles.
        {
            const auto &items = layout.items;
            uc::Fill(context, items, uc::color::Panel);
            const int cols = TileColumns(items);
            for (size_t i = 0; i < folder.items.size(); ++i) {
                const auto           &item = folder.items[i];
                const sky::ui::UIRect tile = TileRect(items, static_cast<int>(i), cols);
                if (tile.top > items.bottom) {
                    break;
                }
                if (selected == item.uuid) {
                    uc::Fill(context, tile, uc::color::RowSelected);
                }

                // Thumbnail seam: real image when a provider supplies one, else a type-tinted icon.
                std::string thumbKey;
                uint32_t    iconColor = TypeIconColor(item.type);
                if (auto *provider = AssetThumbnailProviderRegistry::Get()->GetProvider(); provider != nullptr) {
                    if (provider->GetThumbnail(item.uuid, item.type, thumbKey) && !thumbKey.empty()) {
                        iconColor = uc::color::AccentSoft;
                    }
                }
                const sky::ui::UIRect icon{tile.left + (kTileWidth - kIconSize) * 0.5f, tile.top + 6.0f, tile.left + (kTileWidth + kIconSize) * 0.5f,
                                           tile.top + 6.0f + kIconSize};
                uc::RoundedRect(context, icon, iconColor, 4.0f);
                if (item.state != AssetCookState::NotCooked) {
                    uc::RoundedRect(context, {icon.left + 2.0f, icon.top + 2.0f, icon.left + 9.0f, icon.top + 9.0f}, CookStateColor(item.state),
                                    2.0f);
                }

                const sky::ui::UIRect nameRect{tile.left + 2.0f, icon.bottom + 2.0f, tile.right - 2.0f, tile.bottom - 2.0f};
                if (editing && item.uuid == editingUuid) {
                    skin.DrawField(context, nameRect, true, false);
                    uc::Text(context, edit.GetText(), kTextSize, nameRect, uc::color::Text, textSystem, uc::HAlign::Center);
                } else {
                    uc::Text(context, item.name, kTextSize, nameRect, uc::color::Text, textSystem, uc::HAlign::Center);
                }
            }
        }

        // Detail (bottom).
        {
            const auto &detail = layout.detail;
            uc::Fill(context, detail, uc::color::Section);
            EditorAssetItem selectedItem;
            bool            hasSelection = false;
            for (const auto &item : folder.items) {
                if (item.uuid == selected) {
                    selectedItem = item;
                    hasSelection = true;
                    break;
                }
            }

            float y       = detail.top + 4.0f;
            auto  nextRow = [&](float h) {
                sky::ui::UIRect r{detail.left + 6.0f, y, detail.right - 6.0f, y + h};
                y += h;
                return r;
            };

            if (!hasSelection) {
                uc::Text(context, "(no asset selected)", kTextSize, nextRow(kRowHeight), uc::color::TextMuted, textSystem);
            } else {
                uc::Text(context, selectedItem.name + "  [" + selectedItem.type + "]", kTextSize, nextRow(kRowHeight), uc::color::Text, textSystem);
                uc::Text(context, selectedItem.path, kTextSize, nextRow(kRowHeight), uc::color::TextMuted, textSystem);

                // Target selector (click to cycle) + active platform on one compact row.
                {
                    const sky::ui::UIRect r     = nextRow(kRowHeight);
                    AssetCookState        state = AssetCookState::NotCooked;
                    for (const auto &ti : detailTargets) {
                        if (ti.target == detailTarget) {
                            state = ti.state;
                            break;
                        }
                    }
                    const auto        cookCfg = catalog->GetCookConfig(selectedItem.uuid);
                    const std::string line =
                        "target: < " + (detailTarget.empty() ? std::string("(none)") : detailTarget) + " >  " + CookStateLabel(state) + "   /   " +
                        (cookCfg.activePlatform.empty() ? std::string("platform: (unset)") : "platform: " + cookCfg.activePlatform);
                    uc::Text(context, line, kTextSize, r, selectedItem.writable ? uc::color::Text : uc::color::TextMuted, textSystem);
                }

                uc::Text(context, "double-click to open the asset viewer (cook settings)", kTextSize, nextRow(kRowHeight), uc::color::TextDisabled,
                         textSystem);
                if (!selectedItem.writable) {
                    uc::Text(context, "(read-only mount)", kTextSize, nextRow(kRowHeight), uc::color::TextDisabled, textSystem);
                }
            }
        }

        // Context menu overlay.
        if (menuOpen) {
            skin.DrawPopup(context, menuRect);
            float y = menuRect.top + 3.0f;
            for (int i = 0; i < static_cast<int>(menuEntries.size()); ++i) {
                const auto           &entry    = menuEntries[i];
                const sky::ui::UIRect itemRect = {menuRect.left + 2.0f, y, menuRect.right - 2.0f, y + kRowHeight};
                skin.DrawPopupItem(context, itemRect, i == hoverMenuIndex, false);
                uc::Text(context, entry.label, kTextSize, itemRect, entry.enabled ? uc::color::Text : uc::color::TextDisabled, textSystem);
                y += kRowHeight;
            }
        }
    }

    sky::ui::UIEventResult AssetBrowserPanel::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (menuOpen) {
                const int hover = MenuIndexAt(event.x, event.y);
                if (hover != hoverMenuIndex) {
                    hoverMenuIndex = hover;
                    MarkPaintDirty();
                }
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::DOWN && event.button == 1) {
            // Right-click over the item grid opens the context menu for the hit item.
            const Layout layout = ComputeLayout();
            const int    index  = TileAt(layout.items, event.x, event.y, TileColumns(layout.items), static_cast<int>(folder.items.size()));
            if (index >= 0) {
                SelectItem(folder.items[index]);
                OpenStreamMenu(event.x, event.y, folder.items[index]);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (menuOpen) {
                menuOpen = false;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::UP || event.button != 0) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (HandleMenuClick(event.x, event.y)) {
            return sky::ui::UIEventResult::HANDLED;
        }

        const Layout layout = ComputeLayout();

        // Detail pane: editable per-target cook settings.
        if (event.x >= layout.detail.left && event.x <= layout.detail.right && event.y >= layout.detail.top && event.y <= layout.detail.bottom) {
            EditorAssetItem item;
            bool            hasItem = false;
            for (const auto &it : folder.items) {
                if (it.uuid == selected) {
                    item    = it;
                    hasItem = true;
                    break;
                }
            }

            const int drow = static_cast<int>((event.y - layout.detail.top - 4.0f) / kRowHeight);
            if (catalog != nullptr && hasItem && item.writable && drow == 3) {
                // Cycle the edited target.
                if (detailTargets.size() > 1) {
                    const auto   it   = std::find_if(detailTargets.begin(), detailTargets.end(),
                                                     [this](const EditorAssetTargetInfo &info) { return info.target == detailTarget; });
                    const size_t next = it == detailTargets.end() ? 0 : (std::distance(detailTargets.begin(), it) + 1) % detailTargets.size();
                    detailTarget      = detailTargets[next].target;
                    ReloadDetail();
                }
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        // Tree region: toggle expand (if it has children) and navigate.
        if (event.x >= layout.tree.left && event.x <= layout.tree.right && event.y >= layout.tree.top && event.y <= layout.tree.bottom) {
            const int index = static_cast<int>((event.y - layout.tree.top - 2.0f) / kRowHeight);
            if (index >= 0 && index < static_cast<int>(treeCache.size())) {
                const TreeRow &row = treeCache[index];
                if (row.hasChildren) {
                    if (auto it = expanded.find(row.path); it != expanded.end()) {
                        expanded.erase(it);
                    } else {
                        expanded.insert(row.path);
                    }
                }
                Navigate(row.path);
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        // Item grid: select (and open on double-click).
        {
            const int index = TileAt(layout.items, event.x, event.y, TileColumns(layout.items), static_cast<int>(folder.items.size()));
            if (index >= 0) {
                const auto &item     = folder.items[index];
                const auto  now      = std::chrono::steady_clock::now();
                const bool  isDouble = lastIndex == index && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime).count() < 400;
                lastIndex            = index;
                lastClickTime        = now;

                SelectItem(item);
                if (isDouble && openHandler) {
                    openHandler(item.uuid);
                }
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        return sky::ui::UIEventResult::UNHANDLED;
    }

} // namespace sky::editor
