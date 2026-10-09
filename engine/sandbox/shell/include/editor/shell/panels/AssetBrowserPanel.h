//
// Created on 2026/10/08.
//

#pragma once

#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/selection/SelectionService.h>
#include <editor/core/text/TextEditState.h>
#include <editor/shell/PanelView.h>
#include <editor/shell/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace sky::ui {
    class UITextSystem;
}

namespace sky::editor {

    // Asset browser panel: a folder tree on the left (virtual-path tree from the asset catalog)
    // and a list of the selected folder's assets on the right, plus a detail area and a context
    // menu built from the registered asset actions. Renders with the in-house sky::ui toolkit.
    class AssetBrowserPanel : public sky::ui::UIElement, public IPanelChrome {
    public:
        using ActionHandler = std::function<bool(const std::string &actionId)>;
        using OpenHandler   = std::function<void(const Uuid &uuid)>;
        using RenameHandler = std::function<void(const std::string &from, const std::string &to)>;

        explicit AssetBrowserPanel(std::string inTitle = "Assets");
        ~AssetBrowserPanel() override = default;

        const char *GetTypeName() const override
        {
            return "AssetBrowserPanel";
        }

        void SetTextSystem(sky::ui::UITextSystem *text)
        {
            textSystem = text;
        }
        void SetCatalog(EditorAssetCatalog *value)
        {
            catalog = value;
        }
        void SetSelection(SelectionService *value)
        {
            selection = value;
        }
        void SetActionHandler(ActionHandler handler)
        {
            actionHandler = std::move(handler);
        }
        void SetOpenHandler(OpenHandler handler)
        {
            openHandler = std::move(handler);
        }
        void SetRenameHandler(RenameHandler handler)
        {
            renameHandler = std::move(handler);
        }

        // Start an inline rename/move edit for the selected asset (F2 or the Rename action).
        void BeginRename();

        void SetTitleBarVisible(bool visible) override
        {
            titleVisible = visible;
            MarkPaintDirty();
        }

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    private:
        struct TreeRow {
            std::string path;
            std::string name;
            int         depth       = 0;
            bool        hasChildren = false;
            bool        expanded    = false;
        };

        struct MenuEntry {
            std::string id;
            std::string label;
            bool        enabled = true;
        };

        struct Layout {
            sky::ui::UIRect content;
            sky::ui::UIRect tree;
            sky::ui::UIRect items;
            sky::ui::UIRect detail;
            float           rowHeight = 20.0f;
        };

        Layout ComputeLayout() const;

        // Rebuild the cached view (folder tree + items) from the catalog.
        void RefreshView();
        void RebuildTree();
        void FlattenTree(const EditorAssetFolder &node, int depth, bool isRoot);
        void ExpandAncestors(const std::string &path);
        void Navigate(const std::string &path);
        void SelectItem(const EditorAssetItem &item);

        void OpenStreamMenu(float x, float y, const EditorAssetItem &item);
        int  MenuIndexAt(float x, float y) const;
        bool HandleMenuClick(float x, float y);

        void CommitEdit();
        void CancelEdit();

        std::string            title;
        bool                   titleVisible = true;
        sky::ui::UITextSystem *textSystem   = nullptr;
        EditorAssetCatalog    *catalog      = nullptr;
        SelectionService      *selection    = nullptr;
        ActionHandler          actionHandler;
        OpenHandler            openHandler;
        RenameHandler          renameHandler;

        std::string                        currentPath;
        std::unique_ptr<EditorAssetFolder> cachedTree;
        std::vector<EditorAssetFolder>     rootsCache;
        std::vector<TreeRow>               treeCache;
        std::set<std::string>              expanded;
        EditorAssetFolder                  folder; // current folder (folders + items)
        Uuid                               selected;
        uint64_t                           seenRevision = 0;

        // Double-click tracking (index within the item list).
        int                                   lastIndex = -1;
        std::chrono::steady_clock::time_point lastClickTime{};

        bool viewInit = false;

        // Context menu state.
        bool                   menuOpen = false;
        sky::ui::UIRect        menuRect;
        std::vector<MenuEntry> menuEntries;
        int                    hoverMenuIndex = -1;

        // Inline rename/move edit state.
        bool          editing = false;
        Uuid          editingUuid;
        TextEditState edit;

        // Detail-pane info + target selector (cook settings are edited in the asset viewer).
        void                               ReloadDetail();
        std::string                        detailTarget;
        std::vector<EditorAssetTargetInfo> detailTargets;
    };

} // namespace sky::editor
