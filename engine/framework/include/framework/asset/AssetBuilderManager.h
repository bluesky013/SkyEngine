//
// Created by blues on 2024/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/CookConfig.h>
#include <framework/asset/ICookRunner.h>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

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
        void SetForceInProcess(bool force)
        {
            forceInProcess = force;
        }

        const NativeFileSystemPtr &GetEngineFs() const
        {
            return engineFs;
        }
        const NativeFileSystemPtr &GetWorkSpaceFs() const
        {
            return workSpaceFs;
        }

        void RegisterBuilder(AssetBuilder *builder);
        void UnRegisterBuilder(AssetBuilder *builder);

        void LoadBuildConfigs(const FileSystemPtr &fs);

        Any  GetImportConfig(const FilePath &request);
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

        // Effective settings description for an extension's builder and product bundle (empty when
        // the extension is unknown or the builder reports none).
        std::vector<std::pair<std::string, std::string>> GetBuilderSettings(const std::string &ext, const ProductBundleKey &bundle) const;

        // Effective settings with a per-asset sparse override applied over the bundle preset.
        std::vector<std::pair<std::string, std::string>>
        GetBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const BuildSettingsOverride &override) const;

        // The builder's reflected cook-settings type (nullptr when unknown / none).
        const TypeInfoRT *GetBuilderSettingsType(const std::string &ext) const;
        // Materialize a builder's effective settings for one target into a reflected Any.
        Any MakeBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const BuildSettingsOverride &override) const;
        // Sparse override reproducing an edited reflected settings object vs the bundle preset.
        BuildSettingsOverride DiffBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const Any &edited) const;

    private:
        std::vector<std::unique_ptr<AssetBuilder>>      assetBuilders;
        std::unordered_map<std::string, AssetBuilder *> assetBuilderMap;

        NativeFileSystemPtr          engineFs;
        NativeFileSystemPtr          workSpaceFs;
        NativeFileSystemPtr          intermediateFs;
        CookConfig                   config;
        mutable std::mutex           configMutex;
        std::unique_ptr<ICookRunner> cookRunner;
        bool                         forceInProcess     = false;
        bool                         outOfProcessActive = false;
    };

} // namespace sky
