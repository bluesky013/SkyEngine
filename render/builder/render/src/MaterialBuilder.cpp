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
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/ImageAsset.h>

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
                    Uuid texID;
                    if (am->QueryOrImportSource(obj.GetString(), {ImageBuilder::KEY.data()}, texID)) {
                        properties.valueMap[iter->name.GetString()] = MaterialTexture{static_cast<uint32_t>(properties.images.size())};
                        properties.images.emplace_back(am->LoadAsset<Texture>(texID));
                    }
                } else if (obj.IsFloat()) {
                    properties.valueMap[iter->name.GetString()] = obj.GetFloat();
                } else if (obj.IsArray()) {
                    auto array = obj.GetArray();
                    float *v = nullptr;
                    if (array.Size() == 2) {
                        v = properties.valueMap.emplace(iter->name.GetString(), Vector2{}).first->second.GetAs<Vector2>()->v;
                    } else if (array.Size() == 3) {
                        v = properties.valueMap.emplace(iter->name.GetString(), Vector3{}).first->second.GetAs<Vector3>()->v;
                    } else if (array.Size() == 4) {
                        v = properties.valueMap.emplace(iter->name.GetString(), Vector4{}).first->second.GetAs<Vector4>()->v;
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

        std::string json;
        ReadString(request.fullPath, json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("techniques")) {
            return;
        }

        AssetManager *am = AssetManager::Get();
        std::filesystem::path fullPath(request.fullPath);
        std::filesystem::path outPath(request.outDir);
        outPath.append("materials");
        std::filesystem::create_directories(outPath);
        outPath.append(request.name);

        auto asset = am->CreateAsset<Material>(outPath.make_preferred().string());
        auto &assetData = asset->Data();

        auto &val = document["techniques"];
        if (val.IsArray()) {
            auto techArray = val.GetArray();
            for (auto &tech : techArray) {
                if (tech.IsString()) {
                    Uuid techId;
                    if (am->QueryOrImportSource(tech.GetString(), {TechniqueBuilder::KEY.data()}, techId)) {
                        assetData.techniques.emplace_back(am->LoadAsset<Technique>(techId));
                    }
                }
            }
        }

        ProcessProperties(document, assetData.defaultProperties, request);

        result.products.emplace_back(BuildProduct{KEY.data(), asset->GetUuid()});
        am->SaveAsset(asset);
    }
}
