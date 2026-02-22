//
// Created by blues on 2024/8/5.
//

#include <builder/render/AnimationBuilder.h>

namespace sky::builder {

    void AnimationBuilder::RequestClip(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<AnimationClip>(request.assetInfo->uuid);
        auto archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto &data = asset->Data();
        data.Load(bin);

        for (auto& channel : data.nodeChannels) {
            channel.Compress();
        }

        asset->ResetDependencies();

        if (data.skeleton) {
            asset->AddDependencies(data.skeleton);
        }

        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }
    void AnimationBuilder::RequestGraph(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<Animation>(request.assetInfo->uuid);
        auto archive = request.file->ReadAsArchive();
        JsonInputArchive json(*archive);

        auto &data = asset->Data();
        data.LoadJson(json);
    }

    void AnimationBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.assetInfo->ext == ".clip") {
            RequestClip(request, result);
        } else if (request.assetInfo->ext == ".graph") {
            RequestGraph(request, result);
        }
    }

} // namespace sky::builder