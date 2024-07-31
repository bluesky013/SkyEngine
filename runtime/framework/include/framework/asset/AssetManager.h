//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/Asset.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/asset/AssetExecutor.h>

#include <unordered_map>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        AssetManager() = default;
        ~AssetManager() override = default;

        void SetWorkFileSystem(const FileSystemPtr &fs);
        void AddAssetProductBundle(AssetProductBundle *bundle);

        AssetPtr FindAsset(const Uuid &uuid) const;
        AssetPtr FindOrCreateAsset(const Uuid &uuid, const std::string &type);

        AssetPtr LoadAsset(const Uuid &uuid);
        void SaveAsset(const AssetPtr &asset, const ProductBundleKey &bundleKey);

        AssetPtr LoadAssetFromPath(const std::string &path);

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
            return std::static_pointer_cast<Asset<T>>(FindOrCreateAsset(uuid, std::string(AssetTraits<T>::ASSET_TYPE)));
        }

        FilePtr OpenFile(const Uuid &uuid) const;

        void RegisterAssetHandler(const std::string_view &type, AssetHandlerBase *handler);
        template <class T>
        void RegisterAssetHandler()
        {
            RegisterAssetHandler(AssetTraits<T>::ASSET_TYPE, new AssetHandler<T>());
        }
    private:
        FileSystemPtr workSpace;
        AssetPtr CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive);
        AssetProductBundle *GetBundle(const ProductBundleKey &key) const;

        std::unordered_map<std::string, std::unique_ptr<AssetHandlerBase>> assetHandlers;
        std::vector<std::unique_ptr<AssetProductBundle>> bundles;

        mutable std::recursive_mutex mutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assets;
    };

} // namespace sky