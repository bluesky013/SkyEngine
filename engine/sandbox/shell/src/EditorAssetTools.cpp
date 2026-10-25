#include <editor/shell/EditorShell.h>

#include <algorithm>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/shell/AssetViewerWidget.h>
#include <editor/ui/UiTheme.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>

namespace sky::editor {
    void EditorShell::SetAssetToolHost(std::function<uint32_t(const std::string &)> create, std::function<void(uint32_t)> retire)
    {
        createAssetTool = std::move(create);
        retireAssetTool = std::move(retire);
    }
    bool EditorShell::RequestCloseAssetTool(uint32_t surfaceId)
    {
        const auto found = surfaces.find(surfaceId);
        if (found == surfaces.end() || !found->second.assetTool)
            return false;
        found->second.assetTool->Close();
        return true;
    }
    void EditorShell::OpenPendingAssetTools()
    {
        auto pending = std::move(pendingAssetTools);
        pendingAssetTools.clear();
        for (const auto &asset : pending) {
            if (std::ranges::any_of(surfaces, [&](const auto &entry) { return entry.second.assetTool && entry.second.asset == asset; }))
                continue;
            EditorAssetItem item;
            if (!createAssetTool || !assetCatalog || !assetCatalog->Find(asset, item)) {
                interactionStatus = "Asset document is unavailable";
                continue;
            }
            const auto id = createAssetTool(item.name);
            if (!id || surfaces.contains(id)) {
                interactionStatus = "Cannot create asset tool window";
                continue;
            }
            Surface surface;
            surface.actionLifetime      = std::make_shared<int>(0);
            surface.context             = std::make_unique<ui::UIContext>();
            surface.router              = std::make_unique<ui::UIEventRouter>(*surface.context);
            surface.asset               = asset;
            surface.actionScope         = "assetTool." + asset.ToString();
            surface.context->GetTheme() = context->GetTheme();
            const auto &metrics         = GetDefaultUiTheme().metrics;
            surface.context->SetContentSize(metrics.fieldWidth * 6, metrics.rowHeight * 35);
            auto viewer = std::make_unique<AssetViewerWidget>(textSystem);
            viewer->SetStandalone(true);
            viewer->SetActionScope(surface.actionScope);
            viewer->SetCatalog(assetCatalog);
            viewer->SetActionHandler([](const std::string &action) { return EditorActionRegistry::Get()->Invoke(action); });
            surface.assetTool = viewer.get();
            surface.view      = surface.context->AddChild(std::move(viewer));
            ScopedEditorActions actions(surface.actionScope);
            RegisterAssetViewerActions(surface.assetTool, surface.actionScope, surface.actionLifetime);
            surface.assetTool->Arrange(surface.context->GetRoot()->GetBounds());
            surface.assetTool->Open(asset);
            surfaces.emplace(id, std::move(surface));
        }
    }
} // namespace sky::editor
