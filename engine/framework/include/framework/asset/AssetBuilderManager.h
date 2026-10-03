//
// Created by blues on 2024/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/CookConfig.h>
#include <framework/asset/ICookRunner.h>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>

namespace sky {

    class AssetBuilderManager : public Singleton<AssetBuilderManager> {
    public:
        AssetBuilderManager() = default;
        ~AssetBuilderManager() override;

        void SetWorkSpaceFs(const NativeFileSystemPtr &fs);
        void SetEngineFs(const NativeFileSystemPtr &fs);
        void SetInterMediateFs(const NativeFileSystemPtr &fs);
        // The worker process forces in-process cooking (it IS the worker); must be set
        // before SetWorkSpaceFs so out-of-process selection is skipped (single writer).
        void SetForceInProcess(bool force) { forceInProcess = force; }

        const NativeFileSystemPtr &GetEngineFs() const { return engineFs; }
        const NativeFileSystemPtr &GetWorkSpaceFs() const { return workSpaceFs; }

        void RegisterBuilder(AssetBuilder *builder);
        void UnRegisterBuilder(AssetBuilder *builder);

        void LoadBuildConfigs(const FileSystemPtr &fs);

        Any GetImportConfig(const FilePath &request);
        void ImportAsset(const AssetImportRequest &request);

        // onFinished (optional) runs on the cook-pool thread after the result is broadcast.
        using BuildCompletion = std::function<void(const AssetBuildResult &result)>;

        void BuildRequest(const AssetBuildRequest &request, BuildCompletion onFinished = {});
        void BuildRequest(const Uuid &uuid, const std::string &target, BuildCompletion onFinished = {});
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
        std::unique_ptr<ICookRunner> cookRunner;
        bool forceInProcess = false;
        bool outOfProcessActive = false;
    };

} // namespace sky
