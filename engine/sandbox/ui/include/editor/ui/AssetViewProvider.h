//
// Per-asset-type customization seam for the asset viewer (see asset-viewer-widget). A provider keyed by
// asset type can supply a custom content widget (right region; default = reflected cook settings). The
// left preview region is reserved and served by the separate IAssetPreviewProvider seam (editor/core).
// Widgets are `UIElement`s, so providers live in the UI layer (EditorUi).
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <ui/UIElement.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class IAssetViewProvider {
    public:
        virtual ~IAssetViewProvider() = default;

        // Content for the right region; null -> the default reflected cook-settings form.
        // `text` is the host text system (a content widget draws its own labels/fields).
        virtual std::unique_ptr<sky::ui::UIElement> CreateContent(const Uuid &uuid, const std::string &type, sky::ui::UITextSystem *text)
        {
            (void)uuid;
            (void)type;
            (void)text;
            return nullptr;
        }

        // Flush the content's buffered edits to the source and re-cook (the asset-view base window
        // owns the Save/close action). `content` is the element returned by CreateContent. Returns
        // true when something was committed.
        virtual bool SaveContent(const Uuid &uuid, const std::string &type, sky::ui::UIElement *content)
        {
            (void)uuid;
            (void)type;
            (void)content;
            return false;
        }
    };

    // Process-wide (cross-DLL) registry keyed by asset type.
    class AssetViewProviderRegistry : public sky::Singleton<AssetViewProviderRegistry> {
        friend class sky::Singleton<AssetViewProviderRegistry>;

    public:
        void Register(const std::string &type, IAssetViewProvider *provider)
        {
            providers[type] = provider;
        }
        void Unregister(const std::string &type)
        {
            providers.erase(type);
        }
        IAssetViewProvider *Find(const std::string &type) const
        {
            const auto it = providers.find(type);
            return it != providers.end() ? it->second : nullptr;
        }

    private:
        std::unordered_map<std::string, IAssetViewProvider *> providers;
    };

} // namespace sky::editor
