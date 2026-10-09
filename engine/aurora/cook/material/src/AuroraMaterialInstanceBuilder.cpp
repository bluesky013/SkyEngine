//
// Aurora material-instance builder (see AuroraMaterialInstanceBuilder.h).
//

#include <aurora/cook/material/AuroraMaterialInstanceBuilder.h>

#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/cook/material/MaterialSourceParse.h>

#include <core/logger/Logger.h>

#include <string>
#include <vector>

static const char *TAG = "AuroraMaterialInstanceBuilder";

namespace sky::aurora {

    std::string_view AuroraMaterialInstanceBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<MaterialInstance>::ASSET_TYPE;
    }

    void AuroraMaterialInstanceBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        auto asset = CreateProduct<MaterialInstance>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        auto &data   = asset->Data();
        data.version = MaterialInstanceAssetData::CURRENT_VERSION;

        if (document.HasMember("material") && document["material"].IsString()) {
            const Uuid material = Uuid::CreateFromString(document["material"].GetString());
            if (material) {
                data.material = material;
                AddAssetDependency(asset, request, material);
            } else {
                LOG_W(TAG, "invalid parent material uuid '%s'", document["material"].GetString());
            }
        }

        if (document.HasMember("properties")) {
            std::vector<cook::ParsedProperty> parsed;
            cook::ParseMaterialProperties(document["properties"], parsed);
            for (auto &property : parsed) {
                if (IsTextureProperty(property.type) && property.texture) {
                    MaterialInstanceTextureOverride override;
                    override.name    = property.name;
                    override.texture = property.texture;
                    AddAssetDependency(asset, request, property.texture);
                    data.textureOverrides.push_back(std::move(override));
                } else {
                    MaterialInstanceValueOverride override;
                    override.name  = property.name;
                    override.type  = property.type;
                    override.value = std::move(property.value);
                    data.valueOverrides.push_back(std::move(override));
                }
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built material instance '%s' (%u value, %u texture overrides)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(data.valueOverrides.size()), static_cast<uint32_t>(data.textureOverrides.size()));
    }

} // namespace sky::aurora
