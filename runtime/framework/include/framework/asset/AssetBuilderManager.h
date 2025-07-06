//
// Created by blues on 2024/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetBuilderConfig.h>
#include <queue>

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

        AssetBuilder *QueryBuilder(const std::string &ext) const;

    private:
        std::vector<std::unique_ptr<AssetBuilder>> assetBuilders;
        std::unordered_map<std::string, AssetBuilder*> assetBuilderMap;

        NativeFileSystemPtr engineFs;
        NativeFileSystemPtr workSpaceFs;
        NativeFileSystemPtr intermediateFs;
        AssetBuilderConfig config;
    };

} // namespace sky
