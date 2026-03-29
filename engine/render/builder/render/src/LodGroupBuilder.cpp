//
// Created by blues on 2026/2/17.
//

#include <builder/render/LodGroupBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <core/logger/Logger.h>

static const char *TAG = "LodGroupBuilder";

namespace sky::builder {

    void LodGroupBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<LodGroup>(request.assetInfo->uuid);

        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        auto &data = asset->Data();
        data.LoadJson(json);

        asset->ResetDependencies();

        for (const auto& level : data.levels) {
            // AssetBuilderManager::Get()->BuildRequest(level.resId, request.target);
            asset->AddDependencies(level.resId);
        }

        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
        LOG_I(TAG, "Build LodGroup %s Success", request.assetInfo->uuid.ToString().c_str());
    }

} // namespace sky::builder