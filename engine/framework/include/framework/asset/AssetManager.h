//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/Asset.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/compression/Compressor.h>
#include <framework/asset/AssetExecutor.h>

#include <unordered_map>
#include <unordered_set>

namespace sky {

    class AssetDependencyGraph;
    class ISourceCatalog;

    class AssetManager : public Singleton<AssetManager> {
    public:
        AssetManager() = default;
        ~AssetManager() override = default;

        void SetWorkFileSystem(const FileSystemPtr &fs);
        void SetSourceCatalog(ISourceCatalog *catalog) { sourceCatalog = catalog; }
        // Compress product payloads with the given codec (see CompressionManager). Off by default.
        void SetProductCompression(CompressionMethod method) { compressionMethod = method; compressProducts = true; }
        void DisableProductCompression() { compressProducts = false; }
        void AddAssetProductBundle(AssetProductBundle *bundle);

        AssetPtr FindAsset(const Uuid &uuid) const;
        AssetPtr FindOrCreateAsset(const Uuid &uuid, const Name &type);

        AssetPtr LoadAsset(const Uuid &uuid);
        void SaveAsset(const AssetPtr &asset, const ProductBundleKey &bundleKey);

        AssetPtr LoadAssetFromPath(const std::string &path);

        const FileSystemPtr &GetWorkSpaceFS() const { return workSpace; }

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAssetFromPath(const std::string &path)
        {
            return std::static_pointer_cast<Asset<T>>(LoadAssetFromPath(path));
        }

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid &uuid)
        {
            return std::static_pointer_cast<Asset<T>>(LoadAsset(uuid));
        }

        template <typename T>
        std::shared_ptr<Asset<T>> FindAsset(const Uuid &uuid)
        {
            return std::static_pointer_cast<Asset<T>>(FindAsset(uuid));
        }

        template <typename T>
        std::shared_ptr<Asset<T>> FindOrCreateAsset(const Uuid &uuid)
        {
            return std::static_pointer_cast<Asset<T>>(FindOrCreateAsset(uuid, Name(AssetTraits<T>::ASSET_TYPE.data())));
        }

        FilePtr OpenFile(const Uuid &uuid) const;

        // Build the runtime dependency graph from loaded product assets.
        void BuildDependencyGraph(AssetDependencyGraph &graph) const;

        void RegisterAssetHandler(const std::string_view &type, AssetHandlerBase *handler);
        template <class T>
        void RegisterAssetHandler()
        {
            RegisterAssetHandler(AssetTraits<T>::ASSET_TYPE, new AssetHandler<T>());
        }
    private:
        FileSystemPtr workSpace;
        ISourceCatalog *sourceCatalog = nullptr;
        bool compressProducts = false;
        CompressionMethod compressionMethod = CompressionMethod::LZ4;
        AssetPtr CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive, std::string &codec);
        // Resolve the payload archive, decompressing when the product header records a codec.
        IStreamArchivePtr PreparePayload(const IStreamArchivePtr &archive, const std::string &codec) const;
        // Product missing + source exists: schedule an in-process cook and return a LOADING asset.
        AssetPtr LoadAssetOnDemand(const Uuid &uuid);
        // Synchronous deserialize from an existing product (used by the in-process cook task).
        void DeserializeProduct(const Uuid &uuid);
        // Run the handler load + status transition + loaded event for an asset whose deps are resolved.
        bool LoadInto(const AssetPtr &asset, const IStreamArchivePtr &payload);
        AssetProductBundle *GetBundle(const ProductBundleKey &key) const;

        std::unordered_map<Name, std::unique_ptr<AssetHandlerBase>> assetHandlers;
        std::vector<std::unique_ptr<AssetProductBundle>> bundles;

        // Product path index (canonical logical path -> uuid), assembled from each bundle's product.index.
        std::unordered_map<std::string, Uuid> productPathMap;
        // Per-bundle parsed product indexes (canonical logical path -> uuid).
        AssetIndexFileCache productIndices{"product.index", "path"};
        // Assets with an in-flight on-demand cook (coalesced per uuid).
        std::unordered_set<Uuid> pendingCooks;

        mutable std::recursive_mutex mutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assets;
    };

} // namespace sky