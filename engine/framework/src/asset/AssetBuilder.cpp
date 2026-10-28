//
// Created by Zach Lee on 2023/2/20.
//

#include <framework/asset/AssetBuilder.h>

#include <core/logger/Logger.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>

static const char *TAG = "AssetBuilder";

namespace sky {

    bool AssetBuilder::LoadJsonDocument(const AssetBuildRequest &request, AssetBuildResult &result, rapidjson::Document &document)
    {
        if (request.file == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "no source file";
            LOG_E(TAG, "no source file for %s", request.assetInfo->path.GetStr().c_str());
            return false;
        }

        std::string json;
        request.file->ReadString(json);
        document.Parse(json.c_str());
        if (document.HasParseError()) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "json parse error";
            LOG_E(TAG, "json parse error: %s", request.assetInfo->path.GetStr().c_str());
            return false;
        }
        return true;
    }

    std::shared_ptr<AssetBase> AssetBuilder::FindOrCreateProduct(const Uuid &uuid, const Name &type, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->CreateBuildAsset(uuid, type);
        if (asset == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "failed to create product asset";
            LOG_E(TAG, "failed to create product asset %s", uuid.ToString().c_str());
        }
        return asset;
    }

    void AssetBuilder::AddAssetDependency(const std::shared_ptr<AssetBase> &asset, const AssetBuildRequest &request, const Uuid &id)
    {
        if (!id) {
            return;
        }
        asset->AddDependencies(id);
        if (AssetDataBase::Get()->Exists(id)) {
            request.assetInfo->dependencies.emplace_back(id);
        }
    }

    void AssetBuilder::FinishProduct(const std::shared_ptr<AssetBase> &asset, const AssetBuildRequest &request, AssetBuildResult &result)
    {
        const auto status = AssetManager::Get()->SaveAsset(asset, request.bundle);
        result.retCode    = status ? AssetBuildRetCode::SUCCESS : AssetBuildRetCode::FAILED;
        result.error      = status.message;
    }

} // namespace sky
