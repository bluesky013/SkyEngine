//
// Aurora material-instance builder (see AuroraMaterialInstanceBuilder.h).
//

#include <aurora/cook/material/AuroraMaterialInstanceBuilder.h>

#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>

#include <core/archive/MemoryStreamArchive.h>
#include <core/logger/Logger.h>
#include <framework/serialization/JsonArchive.h>

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
        if (request.file == nullptr) {
            result.error = "no source file";
            return;
        }
        std::string json;
        request.file->ReadString(json);
        sky::IMemoryArchive   memory(json.data(), json.size());
        sky::JsonInputArchive archive(memory);

        auto asset = CreateProduct<MaterialInstance>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        auto &data   = asset->Data();
        data.version = MaterialInstanceAssetData::CURRENT_VERSION;

        MaterialInstanceSource source;
        archive.LoadValueObject(source);
        if (source.material) {
            data.material = source.material;
            AddAssetDependency(asset, request, source.material);
        }

        for (const auto &property : source.properties)
            if (!MaterialPropertyValueMatches(property.type, property.value)) {
                result.error = "Material instance property has an incompatible semantic value type";
                return;
            }

        for (auto &property : source.properties) {
            const Uuid *texture = std::get_if<Uuid>(&property.value);
            if (IsTextureProperty(property.type) && texture != nullptr) {
                MaterialInstanceTextureOverride override;
                override.name    = property.name;
                override.texture = *texture;
                AddAssetDependency(asset, request, *texture);
                data.textureOverrides.push_back(std::move(override));
            } else {
                MaterialInstanceValueOverride override;
                override.name  = property.name;
                override.type  = property.type;
                override.value = std::move(property.value);
                data.valueOverrides.push_back(std::move(override));
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built material instance '%s' (%u value, %u texture overrides)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(data.valueOverrides.size()), static_cast<uint32_t>(data.textureOverrides.size()));
    }

} // namespace sky::aurora
