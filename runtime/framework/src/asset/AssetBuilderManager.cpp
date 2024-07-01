//
// Created by blues on 2024/6/21.
//

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetExecutor.h>
#include <core/file/FileUtil.h>
#include <filesystem>

namespace sky {
    AssetBuilder *AssetBuilderManager::QueryBuilder(const std::string &ext) const
    {
        auto iter = assetBuilderMap.find(ext);
        return iter == assetBuilderMap.end() ? nullptr : iter->second;
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

            uint32_t count = json.StartArray("bundles");
            for (uint32_t i = 0; i < count; ++i) {
                auto bundleFs = productFs->CreateSubSystem(json.LoadString(), true);
                am->AddAssetProductBundle(new HashedAssetBundle(bundleFs, json.LoadString()));
                json.NextArrayElement();
            }
            json.End();

            json.Start("presets");

            json.ForEachMember([&json, this](const std::string &key) {
                uint32_t count = json.StartArray(key);
                auto &preset = presets[key];
                for (uint32_t i = 0; i < count; ++i) {
                    preset.emplace_back(json.LoadString());
                    json.NextArrayElement();
                }

                json.End();
            });

            json.End();
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

} // namespace sky