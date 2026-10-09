//
// Aurora material builder (see AuroraMaterialBuilder.h).
//

#include <aurora/cook/material/AuroraMaterialBuilder.h>

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/cook/material/MaterialSourceParse.h>

#include <core/logger/Logger.h>

#include <string>
#include <vector>

static const char *TAG = "AuroraMaterialBuilder";

namespace sky::aurora {

    std::string_view AuroraMaterialBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<Material>::ASSET_TYPE;
    }

    void AuroraMaterialBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        auto asset = CreateProduct<Material>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        auto &data   = asset->Data();
        data.version = MaterialAssetData::CURRENT_VERSION;

        if (document.HasMember("techniques")) {
            cook::ParseMaterialTechniques(document["techniques"], data.techniques);
        }

        if (document.HasMember("properties")) {
            std::vector<cook::ParsedProperty> parsed;
            cook::ParseMaterialProperties(document["properties"], parsed);
            for (auto &property : parsed) {
                MaterialPropertyData entry;
                entry.name  = property.name;
                entry.type  = property.type;
                entry.value = std::move(property.value);
                if (IsTextureProperty(property.type) && property.texture) {
                    entry.texture = property.texture;
                    AddAssetDependency(asset, request, property.texture);
                }
                data.properties.push_back(std::move(entry));
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built material '%s' (%u techniques, %u properties)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(data.techniques.size()), static_cast<uint32_t>(data.properties.size()));
    }

} // namespace sky::aurora
