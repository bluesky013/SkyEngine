//
// Aurora lod group builder (see AuroraLodGroupBuilder.h).
//

#include <aurora/cook/lodgroup/AuroraLodGroupBuilder.h>

#include <aurora/adaptor/assets/LodGroupAsset.h>

#include <core/logger/Logger.h>

#include <string>

static const char *TAG = "AuroraLodGroupBuilder";

namespace sky::aurora {

    std::string_view AuroraLodGroupBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<LodGroup>::ASSET_TYPE;
    }

    void AuroraLodGroupBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        auto asset = CreateProduct<LodGroup>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        auto &data   = asset->Data();
        data.version = LodGroupAssetData::CURRENT_VERSION;

        if (document.HasMember("levels") && document["levels"].IsArray()) {
            for (const auto &element : document["levels"].GetArray()) {
                if (!element.IsObject()) {
                    continue;
                }
                LodGroupLevelData level;
                if (element.HasMember("screenSize")) {
                    level.screenSize = element["screenSize"].GetFloat();
                }
                if (element.HasMember("mesh") && element["mesh"].IsString()) {
                    const Uuid mesh = Uuid::CreateFromString(element["mesh"].GetString());
                    if (mesh) {
                        level.mesh = mesh;
                        AddAssetDependency(asset, request, mesh);
                    } else {
                        LOG_W(TAG, "invalid mesh uuid '%s'", element["mesh"].GetString());
                    }
                }
                data.levels.push_back(level);
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built lod group '%s' (%u levels)", request.assetInfo->path.GetStr().c_str(), static_cast<uint32_t>(data.levels.size()));
    }

} // namespace sky::aurora
