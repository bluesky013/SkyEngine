//
// Aurora material builder (see AuroraMaterialBuilder.h).
//

#include <aurora/cook/material/AuroraMaterialBuilder.h>

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>

#include <core/archive/MemoryStreamArchive.h>
#include <core/logger/Logger.h>
#include <framework/serialization/JsonArchive.h>

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
        if (request.file == nullptr) {
            result.error = "no source file";
            return;
        }
        std::string json;
        request.file->ReadString(json);
        sky::IMemoryArchive   memory(json.data(), json.size());
        sky::JsonInputArchive archive(memory);

        auto asset = CreateProduct<Material>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        MaterialAssetData data;
        archive.LoadValueObject(data);
        data.version  = MaterialAssetData::CURRENT_VERSION;
        asset->Data() = std::move(data);

        for (auto &property : asset->Data().properties) {
            if (IsTextureProperty(property.type)) {
                if (const Uuid *texture = std::get_if<Uuid>(&property.value); texture != nullptr) {
                    AddAssetDependency(asset, request, *texture);
                }
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built material '%s' (%u techniques, %u properties)", request.assetInfo->path.GetStr().c_str(),
              static_cast<uint32_t>(asset->Data().techniques.size()), static_cast<uint32_t>(asset->Data().properties.size()));
    }

} // namespace sky::aurora
