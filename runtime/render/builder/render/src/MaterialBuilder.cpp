//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/MaterialBuilder.h>

#include <filesystem>
#include <rapidjson/document.h>

#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/logger/Logger.h>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>

static const char* TAG = "MaterialBuilder";

namespace sky::builder {

    void ProcessProperties(rapidjson::Document &document, MaterialProperties &properties, const AssetBuildRequest &request)
    {
        if (!document.HasMember("properties")) {
            return;
        }

        auto &proValues = document["properties"];
        if (proValues.IsObject()) {
            for (auto iter = proValues.MemberBegin(); iter != proValues.MemberEnd(); ++iter) {
                auto &obj = iter->value;

                if (obj.IsString()) {
                    const auto *imagePath = obj.GetString();
                    auto texId = AssetDataBase::Get()->RegisterAsset(imagePath);
                    properties.valueMap[iter->name.GetString()] = MaterialTexture{static_cast<uint32_t>(properties.images.size())};
                    properties.images.emplace_back(texId->uuid);

                    request.assetInfo->dependencies.emplace_back(texId->uuid);
                } else if (obj.IsFloat()) {
                    properties.valueMap[iter->name.GetString()] = obj.GetFloat();
                } else if (obj.IsBool()) {
                    properties.valueMap[iter->name.GetString()] = static_cast<uint32_t>(obj.GetBool());
                } else if (obj.IsArray()) {
                    auto   array = obj.GetArray();
                    float *v     = nullptr;
                    if (array.Size() == 2) {
                        v = std::get<Vector2>(properties.valueMap.emplace(iter->name.GetString(), Vector2{}).first->second).v;
                    } else if (array.Size() == 3) {
                        v = std::get<Vector3>(properties.valueMap.emplace(iter->name.GetString(), Vector3{}).first->second).v;
                    } else if (array.Size() == 4) {
                        v = std::get<Vector4>(properties.valueMap.emplace(iter->name.GetString(), Vector4{}).first->second).v;
                    }
                    for (auto &val : array) {
                        (*v) = val.GetFloat();
                        ++v;
                    }
                }
            }
        }
    }

    void MaterialBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        if (request.assetInfo->ext == ".mat") {
            BuildMaterial(request, result);
        } else if (request.assetInfo->ext == ".mati") {
            BuildMaterialInstance(request, result);
        }
    }

    void MaterialBuilder::BuildMaterialInstance(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::string json;
        request.file->ReadString(json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("material")) {
            return;
        }
        auto *am = AssetManager::Get();
        auto asset = am->FindOrCreateAsset<MaterialInstance>(request.assetInfo->uuid);
        auto &assetData = asset->Data();

        const auto *materialPath = document["material"].GetString();
        auto mat = AssetDataBase::Get()->RegisterAsset(materialPath);
        if (!mat) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        assetData.material = mat->uuid;
        asset->AddDependencies(mat->uuid);

        ProcessProperties(document, assetData.properties, request);
        for (auto &image : assetData.properties.images) {
            asset->AddDependencies(image);
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    void MaterialBuilder::BuildMaterial(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        std::string json;
        request.file->ReadString(json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("techniques")) {
            return;
        }

        auto *am = AssetManager::Get();
        auto asset = am->FindOrCreateAsset<Material>(request.assetInfo->uuid);
        auto &assetData = asset->Data();

        auto &val = document["techniques"];
        if (val.IsArray()) {
            auto techArray = val.GetArray();
            for (auto &tech : techArray) {
                if (tech.IsString()) {
                    const auto *techPath = tech.GetString();
                    auto techId = AssetDataBase::Get()->RegisterAsset(techPath);
                    if (!techId) {
                        LOG_W(TAG, "Material %s import technique failed %s", request.assetInfo->uuid.ToString().c_str(), techPath);
                        continue;
                    }
                    asset->AddDependencies(techId->uuid);

                    request.assetInfo->dependencies.emplace_back(techId->uuid);
                    assetData.techniques.emplace_back(techId->uuid);
                }
            }
        }

        ProcessProperties(document, assetData.defaultProperties, request);
        for (auto &image : assetData.defaultProperties.images) {
            asset->AddDependencies(image);
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }
}
