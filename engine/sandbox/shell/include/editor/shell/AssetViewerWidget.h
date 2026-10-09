//
// Reusable, resizable asset viewer widget (see asset-viewer-widget). A modal overlay showing the asset
// header, a preview region (left) and a content region (right). The content/preview are customizable
// per asset type via AssetViewProviderRegistry; the default content is one reflected cook-settings form
// per resolved target (all targets visible at once).
//

#pragma once

#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/shell/ModalDialog.h>
#include <editor/shell/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <memory>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class ReflectedFormView;

    class AssetViewerWidget : public ModalDialog {
    public:
        explicit AssetViewerWidget(sky::ui::UITextSystem *text);
        ~AssetViewerWidget() override;

        const char *GetTypeName() const override
        {
            return "AssetViewerWidget";
        }

        void SetCatalog(EditorAssetCatalog *value)
        {
            catalog = value;
        }

        void Open(const Uuid &uuid);

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

    private:
        // One reflected settings form per resolved cook target.
        struct TargetPanel {
            std::string        target;
            EditorCookSettings settings;
            ReflectedFormView *view = nullptr;
            float              h    = 0.0f;
        };

        void            RebuildTargetPanels();
        void            Reload();
        void            RebuildCustomContent();
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

        std::vector<TargetPanel> panels;
        sky::ui::UIElement      *customContent = nullptr; // provider content, when any
        sky::ui::UIElement      *customPreview = nullptr; // provider preview, when any

        Uuid        asset;
        std::string assetName;
        std::string assetType;
        std::string assetPath;

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
