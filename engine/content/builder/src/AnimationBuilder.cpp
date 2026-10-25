//
// Content animation builder (see AnimationBuilder.h).
//

#include <content/builder/AnimationBuilder.h>

#include <content/adaptor/assets/AnimationAsset.h>

#include <core/logger/Logger.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/BinaryArchive.h>

#include <array>
#include <string>

static const char *TAG = "ContentAnimationBuilder";

namespace sky::builder {

    std::string_view AnimationBuilder::QueryType(const std::string &ext) const
    {
        if (ext == ".clip") {
            return AssetTraits<AnimationClip>::ASSET_TYPE;
        }
        return {};
    }

    void AnimationBuilder::RequestClip(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.file == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "no source file";
            LOG_E(TAG, "no source file for %s", request.assetInfo->path.GetStr().c_str());
            return;
        }

        auto                   *manager = AssetManager::Get();
        ProductGroupTransaction transaction;
        const auto              beginStatus =
            manager->BeginProductGroup(request.assetInfo->uuid, request.bundle, std::array<Uuid, 1>{request.assetInfo->uuid}, transaction);
        if (!beginStatus) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = beginStatus.message;
            return;
        }
        auto asset = std::static_pointer_cast<Asset<AnimationClip>>(
            manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<AnimationClip>::ASSET_TYPE.data())));
        if (!asset) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "animation clip handler is unavailable";
            return;
        }

        auto               archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto      &data   = asset->Data();
        const auto status = data.LoadSource(bin);
        if (!status) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = status.message;
            LOG_E(TAG, "%s: %s", request.assetInfo->path.GetStr().c_str(), status.message.c_str());
            return;
        }

        for (auto &channel : data.nodeChannels) {
            const auto compressionStatus = channel.Compress();
            if (!compressionStatus) {
                result.error   = compressionStatus.message;
                result.retCode = AssetBuildRetCode::FAILED;
                return;
            }
        }

        // Lay the compressed channels out into the channel table + product bulk.
        auto bulk = data.BuildBulk();
        asset->AddBulk(bulk.data(), bulk.size());

        asset->ResetDependencies();
        if (data.skeleton) {
            AddAssetDependency(asset, request, data.skeleton);
        }

        ProductGroupSnapshotPtr published;
        const auto              saved = manager->SaveProductGroup(transaction, request.bundle, std::array<AssetPtr, 1>{asset}, published);
        if (!saved) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = saved.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
        LOG_I(TAG, "built animation clip '%s' (%u channels)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(data.nodeChannels.size()));
    }

    void AnimationBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        if (request.assetInfo->ext == ".clip") {
            RequestClip(request, result);

        }
    }

} // namespace sky::builder
