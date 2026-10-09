//
// Reusable, resizable asset viewer widget (see asset-viewer-widget).
//

#include <editor/shell/AssetViewerWidget.h>

#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/shell/AssetViewProvider.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <memory>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        constexpr uint32_t kTextSize    = 12;
        constexpr float    kRowHeight   = 20.0f;
        constexpr float    kPad         = 10.0f;
        constexpr float    kMinW        = 480.0f;
        constexpr float    kMinH        = 320.0f;
        constexpr float    kGrip        = 16.0f;
        constexpr float    kPanelGap    = 8.0f;
        constexpr float    kScrollbarW  = 8.0f;
        constexpr float    kTargetFormH = 172.0f; // fallback height when a form has no preferred height
        constexpr float    kCardMaxH    = 160.0f; // cap per-target card so long settings scroll inside the card

    } // namespace

    AssetViewerWidget::AssetViewerWidget(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetFocusable(true);
        SetVisible(false);
    }

    AssetViewerWidget::~AssetViewerWidget() = default;

    void AssetViewerWidget::Open(const Uuid &uuid)
    {
        asset = uuid;
        assetName.clear();
        assetType.clear();
        assetPath.clear();

        if (catalog != nullptr) {
            EditorAssetItem item;
            if (catalog->Find(uuid, item)) {
                assetName = item.name;
                assetType = item.type;
                assetPath = item.path;
            }
        }
        contentScroll = 0.0f;
        RebuildCustomContent();
        RebuildTargetPanels();
        Reload();
        ModalDialog::Open();
    }

    void AssetViewerWidget::RebuildCustomContent()
    {
        contentElements.clear();
        capturedElement = nullptr;
        if (customContent != nullptr) {
            RemoveChild(customContent);
            customContent = nullptr;
        }
        if (customPreview != nullptr) {
            RemoveChild(customPreview);
            customPreview = nullptr;
        }

        IAssetViewProvider *provider = AssetViewProviderRegistry::Get()->Find(assetType);
        if (provider == nullptr) {
            return;
        }
        if (auto content = provider->CreateContent(asset, assetType)) {
            customContent = AddChild(std::move(content));
            AddContentElement(customContent);
        }
        if (auto preview = provider->CreatePreview(asset, assetType)) {
            customPreview = AddChild(std::move(preview));
        }
    }

    void AssetViewerWidget::RebuildTargetPanels()
    {
        for (auto &panel : panels) {
            if (panel.view != nullptr) {
                RemoveChild(panel.view);
            }
        }
        panels.clear();

        // A custom provider content widget replaces the per-target reflected forms.
        if (customContent != nullptr || catalog == nullptr) {
            return;
        }

        const auto targets = catalog->GetTargetInfos(asset);
        for (const auto &info : targets) {
            auto view = std::make_unique<ReflectedFormView>(textSystem, info.target);
            view->SetHeaderVisible(false);
            const std::string target = info.target;
            view->SetOnEdited([this, target]() {
                for (auto &panel : panels) {
                    if (panel.target == target && catalog != nullptr && panel.settings.IsValid()) {
                        catalog->ApplyCookSettings(asset, target, panel.settings.object);
                        break;
                    }
                }
                Reload();
            });
            TargetPanel panel;
            panel.target = target;
            panel.view   = static_cast<ReflectedFormView *>(AddChild(std::move(view)));
            AddContentElement(panel.view);
            panels.push_back(std::move(panel));
        }
    }

    void AssetViewerWidget::Reload()
    {
        if (catalog == nullptr) {
            return;
        }
        for (auto &panel : panels) {
            panel.settings = catalog->GetCookSettings(asset, panel.target);
            if (panel.settings.IsValid()) {
                const PropertyObject object{panel.settings.object.Data(), panel.settings.type};
                const PropertyObject baseline{panel.settings.baseline.Data(), panel.settings.type};
                panel.view->Bind(object, &baseline);
                panel.view->SetVisible(true);
            } else {
                panel.view->Bind(PropertyObject{});
                panel.view->SetVisible(false);
            }
        }
        MarkPaintDirty();
    }

    sky::ui::UIRect AssetViewerWidget::PanelRect() const
    {
        return CenteredPanel(panelW, panelH, kMinW, kMinH);
    }

    sky::ui::UIRect AssetViewerWidget::ContentRect(const sky::ui::UIRect &panel) const
    {
        const float header = skin.Theme().metrics.panelHeaderHeight;
        return {panel.left, panel.top + header, panel.right, panel.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::RightRect(const sky::ui::UIRect &content) const
    {
        const float previewW = (content.right - content.left) * 0.38f;
        return {content.left + previewW + kPad, content.top + kPad, content.right - kPad, content.bottom - kPad};
    }

    sky::ui::UIRect AssetViewerWidget::CloseRect(const sky::ui::UIRect &panel) const
    {
        return {panel.right - 26.0f, panel.top + 6.0f, panel.right - 6.0f, panel.top + 26.0f};
    }

    sky::ui::UIRect AssetViewerWidget::ResizeRect(const sky::ui::UIRect &panel) const
    {
        return {panel.right - kGrip, panel.bottom - kGrip, panel.right, panel.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::ScrollTrackRect() const
    {
        const sky::ui::UIRect right   = RightRect(ContentRect(PanelRect()));
        const float           viewTop = right.top + 2.0f * kRowHeight + 4.0f;
        return {right.right - kScrollbarW, viewTop, right.right, right.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::ScrollThumbRect() const
    {
        const sky::ui::UIRect track     = ScrollTrackRect();
        const float           viewH     = std::max(0.0f, track.bottom - track.top);
        const float           maxScroll = std::max(0.0f, contentTotal - contentViewH);
        const float           span      = contentTotal > 0.0f ? std::min(1.0f, contentViewH / contentTotal) : 1.0f;
        const float           thumbH    = std::max(24.0f, viewH * span);
        const float           t         = maxScroll > 0.0f ? contentScroll / maxScroll : 0.0f;
        const float           thumbTop  = track.top + (viewH - thumbH) * t;
        return {track.left + 1.0f, thumbTop, track.right - 1.0f, thumbTop + thumbH};
    }

    void AssetViewerWidget::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!IsOpen()) {
            return;
        }
        PaintBackdrop(context);

        const sky::ui::UIRect panel   = PanelRect();
        const std::string     title   = assetName.empty() ? std::string("Asset Viewer") : assetName;
        const sky::ui::UIRect content = skin.DrawPanel(context, panel, title, true);

        const float           previewW = (content.right - content.left) * 0.38f;
        const sky::ui::UIRect preview{content.left + kPad, content.top + kPad, content.left + previewW, content.bottom - kPad};
        const sky::ui::UIRect right = RightRect(content);

        // Preview region: a provider widget when present, else a reserved placeholder.
        if (customPreview != nullptr) {
            customPreview->SetBounds(preview);
            customPreview->SetVisible(true);
        } else {
            uc::RoundedField(context, preview, uc::color::Section, uc::color::BorderSoft, 4.0f);
            IAssetPreviewProvider *provider   = AssetPreviewProviderRegistry::Get()->GetProvider();
            const bool             hasPreview = provider != nullptr && provider->GetPreview(asset, assetType);
            uc::Text(context, hasPreview ? "preview (reserved)" : "no preview", kTextSize, preview, uc::color::TextDisabled, textSystem,
                     uc::HAlign::Center, uc::VAlign::Middle);
        }

        float y   = right.top;
        auto  row = [&](float h) {
            const sky::ui::UIRect r{right.left, y, right.right, y + h};
            y += h;
            return r;
        };
        uc::Text(context, assetName + "  [" + assetType + "]", kTextSize, row(kRowHeight), uc::color::Text, textSystem);
        uc::Text(context, assetPath, kTextSize, row(kRowHeight), uc::color::TextMuted, textSystem);

        const float viewTop = y + 4.0f;
        const float viewH   = std::max(0.0f, right.bottom - viewTop);
        contentViewH        = viewH;

        // Size each target form to fit its rows (no inner scrollbar); the right region scrolls as one.
        float total = 0.0f;
        for (auto &tp : panels) {
            const float formH = std::min((tp.view != nullptr) ? tp.view->GetPreferredHeight() : kTargetFormH, kCardMaxH);
            tp.h              = kRowHeight + formH + kPanelGap;
            total += tp.h;
        }
        contentTotal                 = total;
        const float maxScroll        = std::max(0.0f, contentTotal - viewH);
        contentScroll                = std::clamp(contentScroll, 0.0f, maxScroll);
        const bool            hasBar = contentTotal > viewH + 0.5f;
        const float           barW   = hasBar ? kScrollbarW : 0.0f;
        const sky::ui::UIRect contentRight{right.left, right.top, right.right - barW, right.bottom};

        if (customContent != nullptr) {
            customContent->SetBounds(sky::ui::UIRect{contentRight.left, viewTop, contentRight.right, right.bottom});
            customContent->SetVisible(true);
        } else {
            float py = viewTop - contentScroll;
            for (auto &tp : panels) {
                const sky::ui::UIRect labelRect{contentRight.left, py, contentRight.right, py + kRowHeight};
                if (labelRect.bottom > viewTop && labelRect.top < right.bottom) {
                    skin.DrawSectionHeader(context, labelRect, tp.target);
                }
                const float formTop    = std::max(py + kRowHeight, viewTop);
                const float formBottom = std::min(py + kRowHeight + (tp.h - kRowHeight - kPanelGap), right.bottom);
                const bool  visible    = (formBottom > formTop) && tp.settings.IsValid() && labelRect.top < right.bottom && (py + tp.h) > viewTop;
                if (visible) {
                    tp.view->SetBounds(sky::ui::UIRect{contentRight.left, formTop, contentRight.right, formBottom});
                    tp.view->SetVisible(true);
                } else {
                    tp.view->SetVisible(false);
                }
                py += tp.h;
            }
        }

        // Outer scrollbar (hover + drag + wheel): one bar for the whole right region.
        if (hasBar) {
            const sky::ui::UIRect track = ScrollTrackRect();
            uc::RoundedField(context, track, uc::color::Section, uc::color::BorderSoft, kScrollbarW * 0.5f);
            const sky::ui::UIRect thumb      = ScrollThumbRect();
            const auto            thumbColor = (dragScroll || hoverScroll) ? uc::color::Text : uc::color::TextMuted;
            uc::RoundedField(context, thumb, thumbColor, uc::color::BorderSoft, kScrollbarW * 0.5f);
        }

        const sky::ui::UIRect close = CloseRect(panel);
        uc::Text(context, "x", kTextSize, close, hoverClose ? uc::color::Text : uc::color::TextMuted, textSystem, uc::HAlign::Center,
                 uc::VAlign::Middle, true);

        const sky::ui::UIRect grip      = ResizeRect(panel);
        const auto            gripColor = (hoverResize || resizing) ? uc::color::Text : uc::color::TextDisabled;
        uc::Text(context, "//", kTextSize, grip, gripColor, textSystem, uc::HAlign::Center, uc::VAlign::Middle, true);
    }

    sky::ui::UIEventResult AssetViewerWidget::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!IsOpen()) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        const sky::ui::UIRect panel = PanelRect();

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (resizing) {
                panelW = std::clamp(resizeStartW + (event.x - resizeStartX), kMinW, GetBounds().right - 2.0f * kPad);
                panelH = std::clamp(resizeStartH + (event.y - resizeStartY), kMinH, GetBounds().bottom - 2.0f * kPad);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (dragScroll) {
                const float viewH     = std::max(1.0f, contentViewH);
                const float maxScroll = std::max(0.0f, contentTotal - contentViewH);
                contentScroll         = std::clamp(scrollDragStartScroll + (event.y - scrollDragStartY) * (contentTotal / viewH), 0.0f, maxScroll);
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            hoverClose  = CloseRect(panel).Contains(event.x, event.y);
            hoverResize = ResizeRect(panel).Contains(event.x, event.y);
            hoverScroll = contentTotal > contentViewH && ScrollTrackRect().Contains(event.x, event.y);
            MarkPaintDirty();
        } else if (event.action == sky::ui::UIPointerAction::DOWN) {
            if (CloseRect(panel).Contains(event.x, event.y)) {
                Close();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (ResizeRect(panel).Contains(event.x, event.y)) {
                resizing     = true;
                resizeStartX = event.x;
                resizeStartY = event.y;
                resizeStartW = panelW;
                resizeStartH = panelH;
                return sky::ui::UIEventResult::HANDLED;
            }
            if (contentTotal > contentViewH && ScrollTrackRect().Contains(event.x, event.y)) {
                dragScroll            = true;
                scrollDragStartY      = event.y;
                scrollDragStartScroll = contentScroll;
                return sky::ui::UIEventResult::HANDLED;
            }
        } else if (event.action == sky::ui::UIPointerAction::UP) {
            resizing   = false;
            dragScroll = false;
        } else if (event.action == sky::ui::UIPointerAction::WHEEL) {
            // Scroll the hovered child first (nested scrolling); fall back to the outer list.
            if (RouteContentPointer(event) == sky::ui::UIEventResult::HANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const sky::ui::UIRect right = RightRect(ContentRect(panel));
            if (right.Contains(event.x, event.y) && contentTotal > contentViewH) {
                contentScroll = std::clamp(contentScroll - event.wheelDelta * 0.6f, 0.0f, contentTotal - contentViewH);
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        // Forward to content children (hover, drag capture, overlay priority).
        RouteContentPointer(event);
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult AssetViewerWidget::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        constexpr uint32_t kEscape = 0x1B;
        if (event.keyCode == kEscape) {
            Close();
            return sky::ui::UIEventResult::HANDLED;
        }
        return RouteContentKey(event);
    }

    sky::ui::UIEventResult AssetViewerWidget::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        return RouteContentText(event);
    }

} // namespace sky::editor
