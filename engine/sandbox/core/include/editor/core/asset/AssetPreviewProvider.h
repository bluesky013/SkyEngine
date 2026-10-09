//
// Reserved asset preview seam (see asset-viewer-widget). Mirrors the thumbnail seam: the viewer queries
// this hook for its preview region; when no provider is registered it draws a placeholder. Rendering is
// deferred (a future renderer-backed provider targets ViewContentSource::ASSET viewports).
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <string>

namespace sky::editor {

    // Optional provider of asset previews, registered by an extension.
    class IAssetPreviewProvider {
    public:
        virtual ~IAssetPreviewProvider() = default;

        // Returns true when the provider can host/render a preview for the asset (uuid + type).
        virtual bool GetPreview(const Uuid &uuid, const std::string &type) = 0;
    };

    // Process-wide (cross-DLL) holder for the active preview provider.
    class AssetPreviewProviderRegistry : public sky::Singleton<AssetPreviewProviderRegistry> {
        friend class sky::Singleton<AssetPreviewProviderRegistry>;

    public:
        void SetProvider(IAssetPreviewProvider *value)
        {
            provider = value;
        }
        IAssetPreviewProvider *GetProvider() const
        {
            return provider;
        }

    private:
        IAssetPreviewProvider *provider = nullptr;
    };

} // namespace sky::editor
