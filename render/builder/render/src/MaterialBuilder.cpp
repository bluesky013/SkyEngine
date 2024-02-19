//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/MaterialBuilder.h>
#include <filesystem>
#include <rapidjson/document.h>

#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>

#include <builder/render/TechniqueBuilder.h>
#include <builder/render/ImageBuilder.h>

namespace sky::builder {

    void ProcessProperties(rapidjson::Document &document, MaterialProperties &properties, const BuildRequest &request)
    {
        if (!document.HasMember("properties")) {
            return;
        }
        auto *am = AssetManager::Get();

        auto &proValues = document["properties"];
        if (proValues.IsObject()) {
            for (auto iter = proValues.MemberBegin(); iter != proValues.MemberEnd(); ++iter) {
                auto &obj = iter->value;

                if (obj.IsString()) {
                    const auto *imagePath = obj.GetString();
                    auto texId = am->ImportAsset(imagePath);
                    am->BuildAsset(texId, request.targetPlatform);
                    properties.valueMap[iter->name.GetString()] = MaterialTexture{static_cast<uint32_t>(properties.images.size())};
                    properties.images.emplace_back(texId);

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

    void MaterialBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (!request.buildKey.empty() && request.buildKey != std::string(KEY)) {
            return;
        }

        if (request.ext == ".mat") {
            BuildMaterial(request, result);
        } else if (request.ext == ".mati") {
            BuildMaterialInstance(request, result);
        }
    }

    void MaterialBuilder::BuildMaterialInstance(const BuildRequest &request, BuildResult &result)
    {
        std::string json;
        ReadString(request.fullPath, json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("material")) {
            return;
        }
        auto *am = AssetManager::Get();
        auto asset = am->CreateAsset<MaterialInstance>(request.uuid);
        auto &assetData = asset->Data();

        auto materialPath = document["material"].GetString();
        assetData.material = am->ImportAsset(materialPath);
        am->BuildAsset(assetData.material , request.targetPlatform);

        ProcessProperties(document, assetData.properties, request);
        result.products.emplace_back(BuildProduct{KEY.data(), asset, {assetData.material}});
    }

    void MaterialBuilder::BuildMaterial(const BuildRequest &request, BuildResult &result)
    {
        std::string json;
        ReadString(request.fullPath, json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("techniques")) {
            return;
        }

        auto *am = AssetManager::Get();
        std::filesystem::path fullPath(request.fullPath);
        auto asset = am->CreateAsset<Material>(request.uuid);
        auto &assetData = asset->Data();

        std::vector<Uuid> deps;
        auto &val = document["techniques"];
        if (val.IsArray()) {
            auto techArray = val.GetArray();
            for (auto &tech : techArray) {
                if (tech.IsString()) {
                    const auto *techPath = tech.GetString();
                    auto techId = am->ImportAsset(techPath);
                    am->BuildAsset(techId, request.targetPlatform);
                    deps.emplace_back(techId);
                    assetData.techniques.emplace_back(techId);
                }
            }
        }

        ProcessProperties(document, assetData.defaultProperties, request);
        result.products.emplace_back(BuildProduct{KEY.data(), asset, deps});
        result.success = true;
    }
}
