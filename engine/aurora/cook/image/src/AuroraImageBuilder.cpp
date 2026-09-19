//
// Aurora image builder: type mapping and the (stub) cook entry point.
//

#include <aurora/cook/image/AuroraImageBuilder.h>

#include <aurora/adaptor/assets/ImageAsset.h>

#include <core/logger/Logger.h>

static const char *TAG = "AuroraImageBuilder";

namespace sky::aurora {

    std::string_view AuroraImageBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<Texture>::ASSET_TYPE;
    }

    void AuroraImageBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
        // image_build_presets.json (bundle -> format) parsing lands with the
        // per-bundle format policy.
    }

    void AuroraImageBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        // Decode -> resize -> mip -> compress -> ImageAssetData lands in later tasks.
        LOG_W(TAG, "aurora image cook is not implemented yet: %s", request.assetInfo->path.path.GetStr().c_str());
        result.retCode = AssetBuildRetCode::FAILED;
    }

} // namespace sky::aurora
