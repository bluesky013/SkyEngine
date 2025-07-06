//
// Created by blues on 2024/8/10.
//

#include <builder/render/SkeletonBuilder.h>

namespace sky::builder {

    void SkeletonBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<Skeleton>(request.assetInfo->uuid);
        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        auto &data = asset->Data();
        data.LoadJson(json);

        asset->ResetDependencies();
        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
