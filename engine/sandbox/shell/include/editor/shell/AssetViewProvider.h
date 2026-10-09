//
// Per-asset-type customization seam for the asset viewer (see asset-viewer-widget). A provider keyed by
// asset type can supply a custom content widget (right region; default = reflected cook settings) and/or a
// custom preview widget (left region; default = reserved placeholder). Widgets are `UIElement`s, so
// providers live in the shell layer.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace sky::ui {
    class UIElement;
} // namespace sky::ui

namespace sky::editor {

    class IAssetViewProvider {
    public:
        virtual ~IAssetViewProvider() = default;

        // Content for the right region; null -> the default reflected cook-settings form.
        virtual std::unique_ptr<sky::ui::UIElement> CreateContent(const Uuid &uuid, const std::string &type)
        {
            (void)uuid;
            (void)type;
            return nullptr;
        }
        // Content for the left preview region; null -> the reserved placeholder.
        virtual std::unique_ptr<sky::ui::UIElement> CreatePreview(const Uuid &uuid, const std::string &type)
        {
            (void)uuid;
            (void)type;
            return nullptr;
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
