//
// Created on 2026/10/08.
//

#include <editor/shell/panels/AssetBrowserPanel.h>

#include <editor/core/asset/AssetThumbnailProvider.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>

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

        int TileColumns(const sky::ui::UIRect &r, const UiMetrics &m)
        {
            const float inner = r.Width() - m.itemSpacing * 2.0f;
            const int   cols  = static_cast<int>((inner + m.itemSpacing) / (m.assetTileSize + m.itemSpacing));
            return cols < 1 ? 1 : cols;
        }

        sky::ui::UIRect TileRect(const sky::ui::UIRect &r, int index, int cols, const UiMetrics &m)
        {
            const int   row = index / cols;
            const int   col = index % cols;
            const float x   = r.left + m.itemSpacing + static_cast<float>(col) * (m.assetTileSize + m.itemSpacing);
            const float y   = r.top + m.itemSpacing + static_cast<float>(row) * (m.assetTileSize + m.itemSpacing);
            return {x, y, x + m.assetTileSize, y + m.assetTileSize};
        }

        int TileAt(const sky::ui::UIRect &r, float x, float y, int cols, int count, const UiMetrics &m)
        {
            if (!r.Contains(x, y) || x < r.left + m.itemSpacing || y < r.top + m.itemSpacing) {
                return -1;
            }
            const int col = static_cast<int>((x - r.left - m.itemSpacing) / (m.assetTileSize + m.itemSpacing));
            const int row = static_cast<int>((y - r.top - m.itemSpacing) / (m.assetTileSize + m.itemSpacing));
            if (col < 0 || col >= cols || row < 0) {
                return -1;
            }
            const int index = row * cols + col;
            return index < count && TileRect(r, index, cols, m).Contains(x, y) ? index : -1;
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
        const UiTheme        &theme = GetDefaultUiTheme();
        const UiMetrics      &m     = theme.metrics;
        Layout                layout;
        const sky::ui::UIRect content = UiSkin(theme, textSystem).PanelContentRect(GetBounds(), titleVisible);
        layout.content                = content;
        layout.rowHeight              = m.rowHeight;

        const float mid = content.left + (content.right - content.left) * 0.42f;

        // Selection must not move a tile between the two clicks of a double-click.
        const float contentHeight = std::max(0.0f, content.Height());
        const float listHeight    = m.listHeaderHeight + m.itemSpacing * 2.0f + m.assetTileSize;
        const float detailHeight =
            std::min({contentHeight * 0.6f, m.rowHeight * 5.0f + m.panelGap * 2.0f, std::max(0.0f, contentHeight - listHeight)});
        const float listBottom = content.bottom - detailHeight;

        layout.tree   = {content.left, content.top + m.listHeaderHeight, mid - m.checkboxPad, listBottom};
        layout.items  = {mid + m.checkboxPad, content.top + m.listHeaderHeight, content.right, listBottom};
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

        const UiTheme   &theme = GetDefaultUiTheme();
        const UiMetrics &m     = theme.metrics;
        float            width = m.popupMinWidth;
        for (const auto &entry : menuEntries) {
            width = std::max(width, uc::TextWidth(entry.label, theme.fonts.label, textSystem) + m.controlPad * 2.0f);
        }
        const float height = static_cast<float>(menuEntries.size()) * m.popupItemHeight + m.panelGap * 2.0f;
        menuRect           = {x, y, x + width, y + height};
        menuOpen           = true;
        hoverMenuIndex     = -1;
    }

    int AssetBrowserPanel::MenuIndexAt(float x, float y) const
    {
        if (!menuOpen || x < menuRect.left || x > menuRect.right || y < menuRect.top || y > menuRect.bottom) {
            return -1;
        }
        const UiMetrics &m = GetDefaultUiTheme().metrics;
        if (y < menuRect.top + m.panelGap)
            return -1;
        const int index = static_cast<int>((y - menuRect.top - m.panelGap) / m.popupItemHeight);
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
        const UiTheme   &theme    = GetDefaultUiTheme();
        const UiMetrics &m        = theme.metrics;
        const uint32_t   textSize = theme.fonts.label;
        UiSkin           skin(theme, textSystem);
        const auto       content = skin.DrawPanel(context, GetBounds(), title, titleVisible);

        if (catalog == nullptr) {
            uc::Text(context, "(asset catalog unavailable)", textSize,
                     {content.left + m.controlPad, content.top, content.right - m.controlPad, content.top + m.rowHeight}, uc::color::TextMuted,
                     textSystem);
            return;
        }

        // Refresh when the catalog revision advanced (or on first paint).
        if (!viewInit || seenRevision != catalog->GetStateRevision()) {
            RefreshView();
        }

        const Layout layout = ComputeLayout();

        // Breadcrumb.
        {
            const sky::ui::UIRect crumb{content.left, content.top, content.right, content.top + m.listHeaderHeight};
            uc::Fill(context, crumb, uc::color::Section);
            uc::Text(context, currentPath.empty() ? "/" : currentPath, textSize,
                     {crumb.left + m.cellPadding, crumb.top, crumb.right - m.cellPadding, crumb.bottom}, uc::color::Text, textSystem);
        }

        // Folder tree (left): collapsible virtual-path hierarchy.
        {
            const auto &tree = layout.tree;
            uc::Fill(context, tree, uc::color::Panel);
            float y = tree.top + m.checkboxPad;
            for (const auto &row : treeCache) {
                if (y + layout.rowHeight > tree.bottom) {
                    break;
                }
                const sky::ui::UIRect rowRect{tree.left + m.checkboxPad, y, tree.right - m.checkboxPad, y + layout.rowHeight};
                if (row.path == currentPath) {
                    uc::Fill(context, rowRect, uc::color::RowSelected);
                }
                const float       indent = tree.left + m.cellPadding + static_cast<float>(row.depth) * m.indentX;
                const std::string arrow  = row.hasChildren ? (row.expanded ? "v " : "> ") : "  ";
                uc::Text(context, arrow + row.name, textSize, {indent, y, tree.right - m.controlPad, y + layout.rowHeight}, uc::color::Text,
                         textSystem);
                y += layout.rowHeight;
            }
        }

        // Item view (right): thumbnail icon tiles.
        {
            const auto &items = layout.items;
            context.PushClip(items);
            uc::Fill(context, items, uc::color::Panel);
            const int cols = TileColumns(items, m);
            for (size_t i = 0; i < folder.items.size(); ++i) {
                const auto           &item = folder.items[i];
                const sky::ui::UIRect tile = TileRect(items, static_cast<int>(i), cols, m);
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
                const sky::ui::UIRect icon{tile.left + (m.assetTileSize - m.assetThumbnailSize) * 0.5f, tile.top + m.cellPadding,
                                           tile.left + (m.assetTileSize + m.assetThumbnailSize) * 0.5f,
                                           tile.top + m.cellPadding + m.assetThumbnailSize};
                uc::RoundedRect(context, icon, iconColor, m.rowRadius);
                if (item.state != AssetCookState::NotCooked) {
                    uc::RoundedRect(
                        context, {icon.left + m.checkboxPad, icon.top + m.checkboxPad, icon.left + m.iconSize * 0.5f, icon.top + m.iconSize * 0.5f},
                        CookStateColor(item.state), m.checkboxRadius);
                }

                const sky::ui::UIRect nameRect{tile.left + m.checkboxPad, icon.bottom + m.checkboxPad, tile.right - m.checkboxPad,
                                               tile.bottom - m.checkboxPad};
                if (editing && item.uuid == editingUuid) {
                    skin.DrawField(context, nameRect, true, false);
                    uc::Text(context, edit.GetText(), textSize, nameRect, uc::color::Text, textSystem, uc::HAlign::Center);
                } else {
                    uc::Text(context, item.name, textSize, nameRect, uc::color::Text, textSystem, uc::HAlign::Center);
                }
            }
            context.PopClip();
        }

        // Detail (bottom).
        {
            const auto &detail = layout.detail;
            context.PushClip(detail);
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

            float y       = detail.top + m.panelGap;
            auto  nextRow = [&](float h) {
                sky::ui::UIRect r{detail.left + m.cellPadding, y, detail.right - m.cellPadding, y + h};
                y += h;
                return r;
            };

            if (!hasSelection) {
                uc::Text(context, "(no asset selected)", textSize, nextRow(layout.rowHeight), uc::color::TextMuted, textSystem);
            } else {
                uc::Text(context, selectedItem.name + "  [" + selectedItem.type + "]", textSize, nextRow(layout.rowHeight), uc::color::Text,
                         textSystem);
                uc::Text(context, selectedItem.path, textSize, nextRow(layout.rowHeight), uc::color::TextMuted, textSystem);

                // Target selector (click to cycle) + active platform on one compact row.
                {
                    const sky::ui::UIRect r     = nextRow(layout.rowHeight);
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
                    uc::Text(context, line, textSize, r, selectedItem.writable ? uc::color::Text : uc::color::TextMuted, textSystem);
                }

                uc::Text(context, "double-click to open the asset viewer (cook settings)", textSize, nextRow(layout.rowHeight),
                         uc::color::TextDisabled, textSystem);
                if (!selectedItem.writable) {
                    uc::Text(context, "(read-only mount)", textSize, nextRow(layout.rowHeight), uc::color::TextDisabled, textSystem);
                }
            }
            context.PopClip();
        }

        // Context menu overlay.
        if (menuOpen) {
            skin.DrawPopup(context, menuRect);
            float y = menuRect.top + m.panelGap;
            for (int i = 0; i < static_cast<int>(menuEntries.size()); ++i) {
                const auto           &entry    = menuEntries[i];
                const sky::ui::UIRect itemRect = {menuRect.left + m.controlPad, y, menuRect.right - m.controlPad, y + m.popupItemHeight};
                skin.DrawPopupItem(context, itemRect, i == hoverMenuIndex, false);
                uc::Text(context, entry.label, textSize, itemRect, entry.enabled ? uc::color::Text : uc::color::TextDisabled, textSystem);
                y += m.popupItemHeight;
            }
        }
    }

    sky::ui::UIEventResult AssetBrowserPanel::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        const UiMetrics &m = GetDefaultUiTheme().metrics;
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
            const int    index  = TileAt(layout.items, event.x, event.y, TileColumns(layout.items, m), static_cast<int>(folder.items.size()), m);
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

            const float rowY = event.y - layout.detail.top - m.panelGap;
            const int   drow = rowY >= 0.0f ? static_cast<int>(rowY / layout.rowHeight) : -1;
            if (catalog != nullptr && hasItem && item.writable && drow == 2) {
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
            const float rowY  = event.y - layout.tree.top - m.checkboxPad;
            const int   index = rowY >= 0.0f ? static_cast<int>(rowY / layout.rowHeight) : -1;
            if (index >= 0 && index < static_cast<int>(treeCache.size()) &&
                layout.tree.top + m.checkboxPad + static_cast<float>(index + 1) * layout.rowHeight <= layout.tree.bottom) {
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
            const int index = TileAt(layout.items, event.x, event.y, TileColumns(layout.items, m), static_cast<int>(folder.items.size()), m);
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
