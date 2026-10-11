//
// Asset-viewer flag derivation (see asset-viewer-widget). Combines the builder's cook-settings
// availability (Framework) with the provider / view-data registries to decide whether the asset
// viewer opens for a type. This is shell-side viewer policy, so it lives with the shell rather
// than with the UI-free provider seam (editor/ui/AssetViewProvider.h).
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::editor {

    // What the asset viewer renders for a type (see asset-viewer-widget).
    //   BUNDLE: the asset's builder exposes reflected cook settings -> per-target forms.
    //   DATA:   a per-asset content view (a registered provider, and/or a reflected view).
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

    // Derives the viewer flags for an asset type: BUNDLE when the type's builder exposes a
    // cook-settings type, DATA when a content provider or view-data provider is registered.
    // NONE (HIDDEN) when neither. An empty/unknown type defaults to BUNDLE (unregistered types
    // still open the viewer). `ext` is the source extension (e.g. ".png").
    AssetViewFlags DeriveAssetViewFlags(const std::string &type, const std::string &ext);

} // namespace sky::editor
