//
// Created on 2026/10/08.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <string>

namespace sky::editor {

    // Optional provider of asset thumbnails, registered by an extension. The browser queries this
    // seam per item; when no provider is registered (or it has no image for the asset), it renders a
    // type-derived icon instead. Real thumbnail rendering is deferred; this only reserves the hook.
    class IAssetThumbnailProvider {
    public:
        virtual ~IAssetThumbnailProvider() = default;

        // Returns true and fills a stable image key when a thumbnail is available for the asset.
        virtual bool GetThumbnail(const Uuid &uuid, const std::string &type, std::string &outKey) = 0;
    };

    // Process-wide (cross-DLL) holder for the active thumbnail provider.
    class AssetThumbnailProviderRegistry : public sky::Singleton<AssetThumbnailProviderRegistry> {
        friend class sky::Singleton<AssetThumbnailProviderRegistry>;

    public:
        void SetProvider(IAssetThumbnailProvider *value)
        {
            provider = value;
        }
        IAssetThumbnailProvider *GetProvider() const
        {
            return provider;
        }

    private:
        IAssetThumbnailProvider *provider = nullptr;
    };

} // namespace sky::editor
