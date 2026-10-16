//
// Reusable, resizable asset viewer widget (see asset-viewer-widget). A modal overlay showing the asset
// header, a preview region (left) and a content region (right). The content/preview are customizable
// per asset type via AssetViewProviderRegistry; the default content is one reflected cook-settings form
// per resolved target (all targets visible at once).
//

#pragma once

#include <editor/core/asset/AssetViewData.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/command/CommandService.h>
#include <editor/shell/AssetViewFlags.h>
#include <editor/shell/ModalDialog.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/widgets/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
    class DockPanel;
} // namespace sky::ui

namespace sky::editor {

    class ReflectedFormView;

    class MenuBar;
    class ToolBar;

    class AssetViewerWidget : public ModalDialog {
    public:
        explicit AssetViewerWidget(sky::ui::UITextSystem *text);
        ~AssetViewerWidget() override;

        const char *GetTypeName() const override
        {
            return "AssetViewerWidget";
        }

        // The catalog is borrowed and must outlive the editing session.
        void SetCatalog(EditorAssetCatalog *value);

        // Routes a chrome action id back to the host (EditorShell::InvokeAction).
        void SetActionHandler(std::function<bool(const std::string &)> handler)
        {
            actionHandler = std::move(handler);
        }

        void            Open(const Uuid &uuid);
        void            Close();
        CommandService &History()
        {
            return assetCommands;
        }
        bool CanUndo() const;
        bool CanRedo() const;
        bool Undo();
        bool Redo();
        // Consume property/command/theme notifications before incremental layout.
        void SynchronizeLayout();

        // Chrome command targets (driven by the scoped actions registered by the host).
        void OpenAsset();
        void SaveAsset();

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &content) override;

    private:
        // One reflected settings form per resolved cook target.
        struct TargetPanel {
            std::string        target;
            EditorCookSettings settings;
            ReflectedFormView *view = nullptr;
            float              h    = 0.0f;
        };

        bool            HasEditingForms() const;
        void            CancelFormInteractions();
        bool            RefreshActions(bool force = false);
        void            RebuildTargetPanels();
        void            Reload();
        void            RebuildCustomContent();
        void            RebuildDataView();
        sky::ui::UIRect PanelRect() const;
        sky::ui::UIRect ContentRect(const sky::ui::UIRect &panel) const;
        sky::ui::UIRect RightRect(const sky::ui::UIRect &content) const;
        sky::ui::UIRect CloseRect(const sky::ui::UIRect &panel) const;
        sky::ui::UIRect ResizeRect(const sky::ui::UIRect &panel) const;
        sky::ui::UIRect ScrollTrackRect() const;
        sky::ui::UIRect ScrollThumbRect() const;

        sky::ui::UITextSystem *textSystem = nullptr;
        EditorAssetCatalog    *catalog    = nullptr;
        UiSkin                 skin;
        uint64_t               themeRevision        = 0;
        std::shared_ptr<void>  actionLifetime       = std::make_shared<int>(0);
        uint64_t               actionChromeRevision = 0;
        std::string            actionChromeScope;
        bool                   refreshingActions       = false;
        uint64_t               actionSessionGeneration = 0;
        bool                   arrangingContent        = false;

        std::function<bool(const std::string &)> actionHandler;

        // Declared before the model/children references so the injected history
        // remains live until their explicit cleanup in the destructor body.
        CommandService             assetCommands;
        CommandService::CallbackId commandChangedId      = 0;
        bool                       pendingSettingsReload = false;
        uint64_t                   catalogRevision       = 0;

        enum class PendingSessionAction { None, Open, Close };
        PendingSessionAction pendingSession = PendingSessionAction::None;
        Uuid                 pendingAsset;
        bool                 hasPendingCatalog = false;
        EditorAssetCatalog  *pendingCatalog    = nullptr;

        std::vector<TargetPanel> panels;
        sky::ui::UIElement      *customContent   = nullptr; // provider content, when any
        sky::ui::UIElement      *contentViewport = nullptr; // clips the scrollable content region
        sky::ui::DockPanel      *topDock         = nullptr; // docks menu/toolbar/body
        sky::ui::UIElement      *bodyArea        = nullptr; // Fill area (preview + content columns)
        MenuBar                 *menuBar         = nullptr; // scoped File/Edit menu
        ToolBar                 *toolBar         = nullptr; // scoped Save/Undo/Redo buttons

        AssetViewData      registeredViewData; // owns the reflected view struct
        ReflectedFormView *dataView = nullptr; // reflected DATA form, when any

        Uuid           asset;
        std::string    assetName;
        std::string    assetType;
        std::string    assetPath;
        AssetViewFlags viewFlags = AssetViewFlags::NONE;

        float contentScroll = 0.0f;
        float contentTotal  = 0.0f;
        float contentViewH  = 0.0f;

        float panelW       = 780.0f;
        float panelH       = 520.0f;
        bool  resizing     = false;
        float resizeStartX = 0.0f;
        float resizeStartY = 0.0f;
        float resizeStartW = 0.0f;
        float resizeStartH = 0.0f;
        bool  hoverClose   = false;
        bool  hoverResize  = false;

        // Outer (stacked-content) scrollbar.
        bool  hoverScroll           = false;
        bool  dragScroll            = false;
        float scrollDragStartY      = 0.0f;
        float scrollDragStartScroll = 0.0f;
    };

} // namespace sky::editor
