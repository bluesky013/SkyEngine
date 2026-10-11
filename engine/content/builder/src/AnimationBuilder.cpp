//
// Content animation builder (see AnimationBuilder.h).
//

#include <content/builder/AnimationBuilder.h>

#include <content/adaptor/assets/AnimationAsset.h>

#include <core/logger/Logger.h>
#include <framework/serialization/BinaryArchive.h>

#include <string>

static const char *TAG = "ContentAnimationBuilder";

namespace sky::builder {

    std::string_view AnimationBuilder::QueryType(const std::string &ext) const
    {
        if (ext == ".clip") {
            return AssetTraits<AnimationClip>::ASSET_TYPE;
        }
        if (ext == ".graph") {
            return AssetTraits<Animation>::ASSET_TYPE;
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

        auto asset = CreateProduct<AnimationClip>(request, result);
        if (!asset) {
            return;
        }

        auto               archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto &data = asset->Data();
        data.LoadSource(bin);
        if (data.version != AnimationClipAssetData::CURRENT_VERSION) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = "unsupported clip version";
            LOG_E(TAG, "unsupported clip version in %s", request.assetInfo->path.GetStr().c_str());
            return;
        }

        for (auto &channel : data.nodeChannels) {
            channel.Compress();
        }

        // Lay the compressed channels out into the channel table + product bulk.
        auto bulk = data.BuildBulk();
        asset->AddBulk(bulk.data(), bulk.size());

        asset->ResetDependencies();
        if (data.skeleton) {
            AddAssetDependency(asset, request, data.skeleton);
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built animation clip '%s' (%u channels)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(data.nodeChannels.size()));
    }

    void AnimationBuilder::RequestGraph(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        auto asset = CreateProduct<Animation>(request, result);
        if (!asset) {
            return;
        }

        auto &data   = asset->Data();
        data.version = AnimationAssetData::CURRENT_VERSION;

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built animation graph '%s'", request.assetInfo->path.GetStr().c_str());
    }

    void AnimationBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        if (request.assetInfo->ext == ".clip") {
            RequestClip(request, result);
        } else if (request.assetInfo->ext == ".graph") {
            RequestGraph(request, result);
        }
    }

} // namespace sky::builder
