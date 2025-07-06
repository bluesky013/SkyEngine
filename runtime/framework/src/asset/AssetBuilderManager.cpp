//
// Created by blues on 2024/6/21.
//

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetEvent.h>
#include <core/file/FileUtil.h>

namespace sky {
    AssetBuilder *AssetBuilderManager::QueryBuilder(const std::string &ext) const
    {
        auto iter = assetBuilderMap.find(ext);
        return iter == assetBuilderMap.end() ? nullptr : iter->second;
    }

    void AssetBuilderManager::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        engineFs = fs;
    }

    void AssetBuilderManager::SetInterMediateFs(const NativeFileSystemPtr &fs)
    {
        intermediateFs = fs;
    }

    void AssetBuilderManager::SetWorkSpaceFs(const NativeFileSystemPtr &fs)
    {
        workSpaceFs = fs;

        // create products directory
        auto productFs = workSpaceFs->CreateSubSystem("products", true);
        auto configFs = workSpaceFs->CreateSubSystem("configs", false);

        auto *am = AssetManager::Get();
        // load configs
        auto file = configFs->OpenFile("asset_build_presets.json");
        if (file) {
            auto archive = file->ReadAsArchive();
            JsonInputArchive json(*archive);
            config.LoadJson(json);

            if (config.bundles.empty()) {
                config.bundles.emplace_back("common");
            }
            for (auto &bundle :config.bundles) {
                auto bundleFs = productFs->CreateSubSystem(bundle, true);
                am->AddAssetProductBundle(new HashedAssetBundle(bundleFs, bundle));
            }
        }
    }

    void AssetBuilderManager::RegisterBuilder(AssetBuilder *builder)
    {
        assetBuilders.emplace_back(builder);
        const auto &extensions = builder->GetExtensions();
        for (const auto &ext : extensions) {
            assetBuilderMap[ext] = assetBuilders.back().get();
        }
    }

    void AssetBuilderManager::LoadBuildConfigs(const FileSystemPtr &fs)
    {
        for (auto &builder : assetBuilders) {
            builder->LoadConfig(fs);
        }
    }

    void AssetBuilderManager::UnRegisterBuilder(AssetBuilder *builder)
    {
        {
            auto iter = std::find_if(assetBuilders.begin(), assetBuilders.end(),
                                     [builder](const auto &val) {
                                         return builder == val.get();
                                     });
            if (iter != assetBuilders.end()) {
                assetBuilders.erase(iter);
            }
        }

        {
            for (auto iter = assetBuilderMap.begin(); iter != assetBuilderMap.end();) {
                if (iter->second == builder) {
                    iter = assetBuilderMap.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
    }

    void AssetBuilderManager::BuildRequest(const Uuid &uuid, const std::string &target)
    {
        auto srcAsset = AssetDataBase::Get()->FindAsset(uuid);
        if (srcAsset) {
            AssetBuildRequest request = {};
            request.assetInfo = srcAsset;
            request.file = AssetDataBase::Get()->OpenFile(srcAsset);
            request.target = target;

            BuildRequest(request);
        }
    }

    void AssetBuilderManager::BuildRequest(const AssetBuildRequest &request)
    {
        AssetExecutor::Get()->PushSavingTask(request.file, [this, request]() {
            auto *builder = QueryBuilder(request.assetInfo->ext);
            request.assetInfo->dependencies.clear();

            AssetBuildResult result = {};
            builder->Request(request, result);

            AsseEvent::BroadCast(request.assetInfo->uuid, &IAssetEvent::OnAssetBuildFinished, result);
        });
    }

    Any AssetBuilderManager::GetImportConfig(const FilePath &filePath)
    {
        auto ext = filePath.Extension();
        auto *builder = QueryBuilder(ext);
        if (builder == nullptr) {
            return Any{};
        }
        return builder->RequireImportSetting(filePath);
    }

    void AssetBuilderManager::ImportAsset(const AssetImportRequest &request)
    {
        auto ext = request.filePath.Extension();
        auto *builder = QueryBuilder(ext);
        if (builder == nullptr) {
            return;
        }

        AssetExecutor::Get()->DependentAsync([request, builder]() {
            builder->Import(request);
        });
    }

} // namespace sky
