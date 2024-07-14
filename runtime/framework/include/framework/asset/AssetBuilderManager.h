//
// Created by blues on 2024/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetExecutor.h>
#include <queue>

namespace sky {

    class AssetBuilderManager : public Singleton<AssetBuilderManager> {
    public:
        AssetBuilderManager() = default;
        ~AssetBuilderManager() override = default;

        void SetWorkSpaceFs(const NativeFileSystemPtr &fs);

        void RegisterBuilder(AssetBuilder *builder);
        void UnRegisterBuilder(AssetBuilder *builder);

        void LoadBuildConfigs(const FileSystemPtr &fs);

        void ImportAsset(const AssetImportRequest &request);

        void BuildRequest(const AssetBuildRequest &request);
        void BuildRequest(const Uuid &uuid, const std::string &target);

        AssetBuilder *QueryBuilder(const std::string &ext) const;

    private:
        std::vector<std::unique_ptr<AssetBuilder>> assetBuilders;
        std::unordered_map<std::string, AssetBuilder*> assetBuilderMap;
        std::unordered_map<std::string, std::vector<std::string>> presets;

        NativeFileSystemPtr workSpaceFs;
    };

} // namespace sky