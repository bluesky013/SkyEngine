//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/Asset.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetProduct.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/compression/Compressor.h>

#include <atomic>
#include <future>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace sky {

    struct AssetBuildResult;
    class AssetDependencyGraph;
    class ICookRunner;
    class ISourceCatalog;
    enum class AssetLoadPolicy { AllowCook, ProductsOnly };

    struct AssetProductInspection {
        std::string       type;
        std::vector<Uuid> dependencies;
        IStreamArchivePtr payload;
        uint32_t          formatVersion = 0;
        uint64_t          checksum      = 0;
        uint64_t          bulkSize      = 0;
    };

    struct AssetProductData {
        Uuid                 uuid;
        std::string          type;
        std::vector<Uuid>    dependencies;
        std::vector<uint8_t> payload;
    };

    class AssetLoadRequest {
    public:
        bool IsResolved() const
        {
            return resolved.load(std::memory_order_acquire);
        }
        AssetPtr GetAsset() const
        {
            return IsResolved() ? asset : nullptr;
        }

    private:
        friend class AssetManager;
        AssetPtr          asset;
        std::atomic<bool> resolved{false};
    };

    class AssetManager : public Singleton<AssetManager> {
    public:
        AssetManager();
        ~AssetManager() override = default;

        void     SetWorkFileSystem(const FileSystemPtr &fs);
        uint64_t GetProductChangeSerial() const
        {
            return productChangeSerial.load(std::memory_order_acquire);
        }
        void SetSourceCatalog(ISourceCatalog *catalog)
        {
            sourceCatalog = catalog;
        }
        // Select the cook backend. Null keeps the built-in inline in-process path.
        void SetCookRunner(ICookRunner *runner);
        // Re-read every bundle's product.index into the path map (out-of-process cook, D11).
        void RefreshProductIndex();
        // Compress product payloads with the given codec (see CompressionManager). Off by default.
        void SetProductCompression(CompressionMethod method)
        {
            compressionMethod = method;
            compressProducts  = true;
        }
        void DisableProductCompression()
        {
            compressProducts = false;
        }
        void AddAssetProductBundle(AssetProductBundle *bundle);
        // Whether a product for the uuid exists in a specific bundle.
        bool HasProduct(const Uuid &uuid, const ProductBundleKey &bundle) const;
        // Offline inspection validates the complete file without loading dependencies or scheduling cook.
        ProductStatus InspectProduct(const Uuid &uuid, const ProductBundleKey &bundle, AssetProductInspection &out) const;

        AssetPtr FindAsset(const Uuid &uuid) const;
        AssetPtr FindOrCreateAsset(const Uuid &uuid, const Name &type);

        AssetPtr LoadAsset(const Uuid &uuid, AssetLoadPolicy policy = AssetLoadPolicy::AllowCook);
        // Includes lookup/validation IO; the host drains AssetExecutor before destroying this manager.
        std::shared_ptr<AssetLoadRequest> LoadAssetAsync(const Uuid &uuid, AssetLoadPolicy policy = AssetLoadPolicy::ProductsOnly);
        ProductStatus                     SaveAsset(const AssetPtr &asset, const ProductBundleKey &bundleKey);
        ProductStatus                     SaveProducts(std::span<const AssetPtr> products, const ProductBundleKey &bundleKey);
        // The producer validates its codec payload without requiring a runtime handler.
        ProductStatus SaveProductData(std::span<const AssetProductData> products, const ProductBundleKey &bundleKey);
        AssetPtr      CreateBuildAsset(const Uuid &uuid, const Name &type) const;

        // Paths normalize lexically; invalid, missing or conflicting logical identities return an invalid UUID.
        Uuid     ResolveProductPath(std::string_view path);
        AssetPtr LoadAssetFromPath(const std::string &path, AssetLoadPolicy policy = AssetLoadPolicy::AllowCook);

        const FileSystemPtr &GetWorkSpaceFS() const
        {
            return workSpace;
        }

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAssetFromPath(const std::string &path, AssetLoadPolicy policy = AssetLoadPolicy::AllowCook)
        {
            const auto asset = LoadAssetFromPath(path, policy);
            return asset && asset->GetType() == Name(AssetTraits<T>::ASSET_TYPE.data()) ? std::static_pointer_cast<Asset<T>>(asset) : nullptr;
        }

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid &uuid, AssetLoadPolicy policy = AssetLoadPolicy::AllowCook)
        {
            const auto asset = LoadAsset(uuid, policy);
            return asset && asset->GetType() == Name(AssetTraits<T>::ASSET_TYPE.data()) ? std::static_pointer_cast<Asset<T>>(asset) : nullptr;
        }

        template <typename T>
        std::shared_ptr<Asset<T>> FindAsset(const Uuid &uuid)
        {
            const auto asset = FindAsset(uuid);
            return asset && asset->GetType() == Name(AssetTraits<T>::ASSET_TYPE.data()) ? std::static_pointer_cast<Asset<T>>(asset) : nullptr;
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
        Uuid                  FindProductPath(std::string_view canonical, bool &ambiguous);
        std::atomic<uint64_t> productChangeSerial{0};
        FileSystemPtr         workSpace;
        ISourceCatalog       *sourceCatalog     = nullptr;
        bool                  compressProducts  = false;
        CompressionMethod     compressionMethod = CompressionMethod::LZ4;
        AssetPtr CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive, std::string &codec, const AssetPtr &destination = {});
        // Resolve the payload archive, decompressing when the product header records a codec.
        IStreamArchivePtr ReadProductPayload(const IStreamArchivePtr &stream,
                                             const std::string       &codec,
                                             const AssetPtr          &asset,
                                             uint64_t                *declaredBulkSize = nullptr) const;
        IStreamArchivePtr PreparePayload(const IStreamArchivePtr &archive, const std::string &codec) const;
        // Resolve an in-flight on-demand cook: refresh the index, deserialize or fail.
        void OnCookFinished(const AssetBuildResult &result);
        void CompleteCookMember(const AssetBuildResult &result);
        // Product missing + source exists: schedule a cook and return a LOADING asset.
        AssetPtr LoadAssetOnDemand(const Uuid &uuid);
        // Synchronous deserialize from an existing product (used by the in-process cook task).
        bool DeserializeProduct(const Uuid &uuid, const AssetPtr &destination, const TaskNodePtr &completion);
        // Run the handler load + status transition + loaded event for an asset whose deps are resolved.
        bool                LoadInto(const AssetPtr &asset, const IStreamArchivePtr &payload);
        void                CompleteLoad(const AssetPtr &asset, bool success);
        AssetProductBundle *GetBundle(const ProductBundleKey &key) const;
        ProductStatus       EncodeProduct(const AssetPtr &asset, std::vector<uint8_t> &out) const;
        ProductStatus       ReadProductHeader(const Uuid &, const IStreamArchivePtr &, AssetProductInspection &, std::string &) const;
        ProductStatus       EncodePayload(const Uuid              &uuid,
                                          std::string_view         type,
                                          std::span<const Uuid>    dependencies,
                                          std::span<const uint8_t> payload,
                                          std::span<const uint8_t> bulk,
                                          std::vector<uint8_t>    &out) const;
        struct EncodedProduct {
            Uuid                 uuid;
            std::vector<uint8_t> bytes;
        };
        ProductStatus PublishProducts(std::span<const EncodedProduct> products, const ProductBundleKey &bundleKey);
        void          InvalidateProducts(std::span<const Uuid> products);

        std::vector<std::shared_ptr<const void>>                    moduleCode;
        std::unordered_map<Name, std::unique_ptr<AssetHandlerBase>> assetHandlers;
        std::vector<std::unique_ptr<AssetProductBundle>>            bundles;

        // Per-bundle parsed product indexes (canonical logical path -> uuid).
        AssetIndexFileCache productIndices{"product.index", "path"};
        // Selected cook backend (null = built-in inline in-process path).
        ICookRunner *cookRunner = nullptr;

        struct PendingCook {
            std::unordered_set<std::string> targets; // in-flight targets for this uuid
            std::string                     path;
            TaskNodePtr                     completion;
            Uuid                            owner;
            AssetPtr                        placeholder;
            Uuid                            requestId;
        };
        std::unordered_map<Uuid, PendingCook> pendingJobs;
        std::unordered_map<std::string, Uuid> pendingOwners;

        mutable std::recursive_mutex mutex;
        // Serializes load orchestration; protects CreateAssetByHeader's header
        // read and the LOADING transition against concurrent loads.
        std::recursive_mutex                               loadMutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assets;
    };

} // namespace sky
