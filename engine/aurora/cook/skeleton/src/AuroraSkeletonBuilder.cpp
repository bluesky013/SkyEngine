//
// Aurora skeleton builder (see AuroraSkeletonBuilder.h).
//

#include <aurora/cook/skeleton/AuroraSkeletonBuilder.h>

#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <framework/asset/AssetManager.h>

#include <core/logger/Logger.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector3.h>

#include <cmath>
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
        result.error.clear();
        if (!request.assetInfo || !request.file) {
            result.error = "skeleton cook request has no source";
            return;
        }
        auto               *manager = AssetManager::Get();
        const auto          bundle  = request.bundle.empty() ? request.target : request.bundle;
        rapidjson::Document document;
        if (!LoadJsonDocument(request, result, document)) {
            return;
        }

        const auto product = manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<sky::Skeleton>::ASSET_TYPE.data()));
        if (!product) {
            result.error = "Skeleton handler is unavailable";
            return;
        }
        const auto asset = std::static_pointer_cast<Asset<sky::Skeleton>>(product);

        asset->ResetDependencies();
        auto &data = asset->Data();
        data.clear();
        data.version = SkeletonAssetData::CURRENT_VERSION;

        if (!document.IsObject() || !document.HasMember("bones") || !document["bones"].IsArray() || document["bones"].Empty() ||
            document["bones"].Size() > ANIM_MAX_BONE) {
            result.error = "skeleton source requires an array of 1..256 bones";
            return;
        }
        const auto validArray = [](const rapidjson::Value &bone, const char *key, uint32_t count) {
            if (!bone.HasMember(key)) {
                return true;
            }
            const auto &array = bone[key];
            if (!array.IsArray() || array.Size() != count) {
                return false;
            }
            for (const auto &value : array.GetArray()) {
                if (!value.IsNumber() || !std::isfinite(value.GetFloat())) {
                    return false;
                }
            }
            return true;
        };
        {
            for (const auto &bone : document["bones"].GetArray()) {
                if (!bone.IsObject() || !bone.HasMember("name") || !bone["name"].IsString() ||
                    (bone.HasMember("parent") && (!bone["parent"].IsInt() || bone["parent"].GetInt() < -1 ||
                                                  bone["parent"].GetInt() >= static_cast<int>(document["bones"].Size()))) ||
                    !validArray(bone, "translation", 3) || !validArray(bone, "scale", 3) || !validArray(bone, "rotation", 4)) {
                    result.error = "invalid skeleton bone identity, parent or TRS array";
                    return;
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

        const auto status = Skeleton::Validate(data);
        if (!status) {
            result.error = status.message;
            return;
        }
        if (const auto saved = manager->SaveAsset(asset, bundle); !saved) {
            result.error = saved.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
        LOG_I(TAG, "built skeleton '%s' (%u bones)", request.assetInfo->path.GetStr().c_str(), static_cast<uint32_t>(data.boneData.size()));
    }

} // namespace sky::aurora
