//
// Created by blues on 2023/12/24.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetPackage.h>
#include <framework/asset/AssetProducts.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetBuilder.h>

namespace sky {

    enum class AssetGroup : uint32_t {
        ENGINE,
        PROJECT,
        CUSTOM
    };

    struct AssetSearchPath {
        std::string path;
        AssetGroup group;
    };

    class AssetManager : public Singleton<AssetManager> {
    public:
        AssetManager() = default;
        ~AssetManager() override;

        void SetWorkPath(const std::string &path);

#ifdef SKY_EDITOR
        // --- editor only ---
        void SetProjectPath(const std::string &path);
        const std::string &GetProjectPath() const { return projectPath; }

        void LoadConfig(const std::string &path);
        const std::vector<AssetSearchPath> &GetSearchPathList() const { return searchPathList; }

        // asset actions
        const Uuid &ImportAsset(const std::string &path);
        const Uuid &ImportAndBuildAsset(const std::string &path, PlatformType target = PlatformType::DEFAULT);
        const Uuid &RegisterAsset(const SourceAssetInfo &info);
        void RemoveAsset(const Uuid &uuid);
        bool BuildAsset(const BuildRequest &request);
        bool BuildAsset(const Uuid &uuid, PlatformType target = PlatformType::DEFAULT);
        void SaveAsset(const Uuid &uuid, PlatformType target = PlatformType::DEFAULT);
        void SaveAsset(const std::shared_ptr<AssetBase> &asset, PlatformType target = PlatformType::DEFAULT);
        const SourceAssetIDMap &GetIDMap() const { return package->GetIDMap(); }

        // asset builders
        void RegisterBuilder(AssetBuilder *builder);
        // --- editor only ---
#endif

        // load asset
        std::string GetAssetPath(const Uuid &uuid);
        std::shared_ptr<AssetBase> LoadAsset(const Uuid &type, const Uuid &uuid, bool async);
        std::shared_ptr<AssetBase> LoadAsset(const Uuid &type, const std::string &path, bool async);
        std::shared_ptr<AssetBase> CreateAsset(const Uuid &type, const Uuid &uuid);

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid &uuid, bool async = false)
        {
            return std::static_pointer_cast<Asset<T>>(LoadAsset(AssetTraits<T>::ASSET_TYPE, uuid, async));
        }

        template <typename T>
        std::shared_ptr<Asset<T>> LoadAsset(const std::string &path, bool async = false)
        {
            return std::static_pointer_cast<Asset<T>>(LoadAsset(AssetTraits<T>::ASSET_TYPE, path, async));
        }

        template <typename T>
        std::shared_ptr<Asset<T>> CreateAsset(const Uuid &uuid)
        {
            return std::static_pointer_cast<Asset<T>>(CreateAsset(AssetTraits<T>::ASSET_TYPE, uuid));
        }

        // asset handlers
        void RegisterAssetHandler(const Uuid &type, AssetHandlerBase *handler);
        AssetHandlerBase *GetAssetHandler(const Uuid &type);
        template <class T>
        void RegisterAssetHandler()
        {
            RegisterAssetHandler(AssetTraits<T>::ASSET_TYPE, new AssetHandler<T>());
        }

        // util functions
        static uint32_t CalculateFileHash(const std::string &loadPath);
        static std::string GetPlatformPrefix(PlatformType platform);
        static std::string GetBuildOutputPath(const std::string &parent, PlatformType platform);
        static Uuid GetUUIDByPath(const std::string &path);
    private:
        std::string workDir;

#ifdef SKY_EDITOR
        std::string projectPath;
        std::string projectAssetPath;
        std::string engineAssetPath;
        std::vector<AssetSearchPath> searchPathList;
        std::unique_ptr<AssetPackage> package;
        std::vector<std::unique_ptr<AssetBuilder>> assetBuilders;
        std::unordered_map<std::string, std::vector<AssetBuilder*>> assetBuilderMap;
#endif

        std::unique_ptr<AssetProducts> products;
        std::unordered_map<Uuid, std::unique_ptr<AssetHandlerBase>> assetHandlers;


        // loaded assets.
        mutable std::mutex assetMutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assetMap;
    };

} // namespace sky