//
// Created by blues on 2024/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/CookConfig.h>
#include <queue>
#include <mutex>

namespace sky {

    class AssetBuilderManager : public Singleton<AssetBuilderManager> {
    public:
        AssetBuilderManager() = default;
        ~AssetBuilderManager() override = default;

        void SetWorkSpaceFs(const NativeFileSystemPtr &fs);
        void SetEngineFs(const NativeFileSystemPtr &fs);
        void SetInterMediateFs(const NativeFileSystemPtr &fs);

        const NativeFileSystemPtr &GetEngineFs() const { return engineFs; }
        const NativeFileSystemPtr &GetWorkSpaceFs() const { return workSpaceFs; }

        void RegisterBuilder(AssetBuilder *builder);
        void UnRegisterBuilder(AssetBuilder *builder);

        void LoadBuildConfigs(const FileSystemPtr &fs);

        Any GetImportConfig(const FilePath &request);
        void ImportAsset(const AssetImportRequest &request);

        void BuildRequest(const AssetBuildRequest &request);
        void BuildRequest(const Uuid &uuid, const std::string &target);
        // Runs the builder inline (used by the in-process on-demand cook path).
        void BuildRequestSync(const Uuid &uuid, const std::string &target);

        // Unified cook/build configuration (bundles, presets, platform targets).
        CookConfig GetCookConfig() const
        {
            std::lock_guard<std::mutex> lock(configMutex);
            return config;
        }
        void SetCookConfig(CookConfig c)
        {
            std::lock_guard<std::mutex> lock(configMutex);
            config = std::move(c);
        }

        AssetBuilder *QueryBuilder(const std::string &ext) const;
        // All extensions claimed by registered builders.
        std::vector<std::string> GetExtensions() const;

    private:
        std::vector<std::unique_ptr<AssetBuilder>> assetBuilders;
        std::unordered_map<std::string, AssetBuilder*> assetBuilderMap;

        NativeFileSystemPtr engineFs;
        NativeFileSystemPtr workSpaceFs;
        NativeFileSystemPtr intermediateFs;
        CookConfig config;
        mutable std::mutex configMutex;
    };

} // namespace sky
