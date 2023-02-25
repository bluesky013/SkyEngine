//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/jobsystem/JobSystem.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilder.h>
#include <mutex>
#include <unordered_map>

namespace sky {

    struct SourceAssetImportOption {
        std::string buildKey;
    };

    class AssetManager : public Singleton<AssetManager> {
    public:
        ~AssetManager();

        template <class T>
        void RegisterAssetHandler()
        {
            RegisterAssetHandler(AssetTraits<T>::ASSET_TYPE, new AssetHandler<T>());
        }


        template <class T>
        std::shared_ptr<Asset<T>> CreateAsset(const std::string &path)
        {
            auto id = Uuid::CreateWithSeed(Fnv1a32(path));
            auto asset = CreateAsset(AssetTraits<T>::ASSET_TYPE, id);
            asset->path = path;
            return std::static_pointer_cast<Asset<T>>(asset);
        }
        std::shared_ptr<AssetBase> CreateAsset(const Uuid &type, const Uuid &uuid);

        template <class T>
        std::shared_ptr<Asset<T>> LoadAsset(const std::string &path, bool async = false)
        {
            auto id = Uuid::CreateWithSeed(Fnv1a32(path));
            return LoadAsset<T>(id, async);
        }

        template <class T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid &uuid, bool async = false)
        {
            return std::static_pointer_cast<Asset<T>>(LoadAsset(AssetTraits<T>::ASSET_TYPE, uuid, async));
        }

        std::shared_ptr<AssetBase> LoadAsset(const Uuid &type, const Uuid &uuid, bool async);

        void SaveAsset(const std::shared_ptr<AssetBase> &asset);

        std::string GetPathByUuid(const Uuid &id);

        void RegisterBuilder(const std::string &key, AssetBuilder *builder);
        void ImportSource(const std::string &path, const SourceAssetImportOption &option);
        bool QueryOrImportSource(const std::string &path, const std::string &key, Uuid &out);

        void RegisterSearchPath(const std::string &path);
        void RegisterAssetHandler(const Uuid &type, AssetHandlerBase *handler);
        AssetHandlerBase *GetAssetHandler(const Uuid &type);

        std::string GetRealPath(const std::string &relative) const;
        const std::vector<std::string> &GetSearchPaths() const { return searchPaths; }

        void Reset(const std::string &dataBase);

    private:
        friend class Singleton<AssetManager>;
        AssetManager() = default;

        void SaveAssets();
        void RegisterAssets();

        std::vector<std::string>                           searchPaths;

        mutable std::mutex assetMutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assetMap;

        std::unordered_map<Uuid, std::unique_ptr<AssetHandlerBase>> assetHandlers;
        std::unordered_map<std::string, std::vector<AssetBuilder *>> assetBuilders;

        mutable std::mutex dbMutex;
        std::unique_ptr<AssetDataBase> dataBase;
        std::unordered_map<Uuid, std::string> pathMap;
    };

} // namespace sky
