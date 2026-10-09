//
// Aurora skeleton builder (see AuroraSkeletonBuilder.h).
//

#include <aurora/cook/skeleton/AuroraSkeletonBuilder.h>

#include <aurora/adaptor/assets/SkeletonAsset.h>

#include <core/logger/Logger.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector3.h>

#include <string>

static const char *TAG = "AuroraSkeletonBuilder";

namespace sky::aurora {

    std::string_view AuroraSkeletonBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<sky::Skeleton>::ASSET_TYPE;
    }

    void AuroraSkeletonBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        auto asset = CreateProduct<sky::Skeleton>(request, result);
        if (!asset) {
            return;
        }

        asset->ResetDependencies();
        auto &data = asset->Data();
        data.clear();
        data.version = SkeletonAssetData::CURRENT_VERSION;

        if (document.HasMember("bones") && document["bones"].IsArray()) {
            for (const auto &bone : document["bones"].GetArray()) {
                if (!bone.IsObject()) {
                    continue;
                }
                BoneData boneData;
                if (bone.HasMember("name") && bone["name"].IsString()) {
                    boneData.name = Name(bone["name"].GetString());
                }
                if (bone.HasMember("parent") && bone["parent"].IsInt()) {
                    const int parent     = bone["parent"].GetInt();
                    boneData.parentIndex = parent < 0 ? static_cast<uint16_t>(0xFFFF) : static_cast<uint16_t>(parent);
                }
                data.boneData.push_back(boneData);

                Transform transform;
                if (bone.HasMember("translation") && bone["translation"].IsArray()) {
                    const auto &array = bone["translation"].GetArray();
                    if (array.Size() == 3) {
                        transform.translation = Vector3{array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat()};
                    }
                }
                if (bone.HasMember("scale") && bone["scale"].IsArray()) {
                    const auto &array = bone["scale"].GetArray();
                    if (array.Size() == 3) {
                        transform.scale = Vector3{array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat()};
                    }
                }
                if (bone.HasMember("rotation") && bone["rotation"].IsArray()) {
                    const auto &array = bone["rotation"].GetArray();
                    if (array.Size() == 4) {
                        // stored as [x, y, z, w]
                        transform.rotation = Quaternion(array[3].GetFloat(), array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat());
                    }
                }
                data.refPos.push_back(transform);
            }
        }

        FinishProduct(asset, request, result);
        LOG_I(TAG, "built skeleton '%s' (%u bones)", request.assetInfo->path.GetStr().c_str(), static_cast<uint32_t>(data.boneData.size()));
    }

} // namespace sky::aurora
