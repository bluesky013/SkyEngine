//
// Per-asset-type customization seam for the asset viewer (see asset-viewer-widget). A provider keyed by
// asset type can supply a custom content widget (right region; default = reflected cook settings). The
// left preview region is reserved and served by the separate IAssetPreviewProvider seam (editor/core).
// Widgets are `UIElement`s, so providers live in the shell layer.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <ui/UIElement.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // What the asset viewer renders for a type (see asset-viewer-widget).
    //   BUNDLE: the asset's builder exposes reflected cook settings -> per-target forms.
    //   DATA:   a per-asset content view (a registered provider here, and/or a reflected view).
    //   NONE = HIDDEN: neither -> the viewer must not open.
    enum class AssetViewFlags : uint32_t {
        NONE   = 0,
        BUNDLE = 1u << 0,
        DATA   = 1u << 1,
        HIDDEN = NONE,
    };

    constexpr AssetViewFlags operator|(AssetViewFlags a, AssetViewFlags b)
    {
        return static_cast<AssetViewFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    constexpr bool HasAssetViewFlag(AssetViewFlags flags, AssetViewFlags bit)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
    }

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

    // Derives the viewer flags for an asset type: BUNDLE when the type's builder exposes a
    // cook-settings type, DATA when a content provider is registered. NONE (HIDDEN) when neither.
    // An empty/unknown type defaults to BUNDLE (unregistered types still open the viewer).
    // `ext` is the source extension (e.g. ".png"); pass the catalog item's path extension.
    AssetViewFlags DeriveAssetViewFlags(const std::string &type, const std::string &ext);

} // namespace sky::editor
