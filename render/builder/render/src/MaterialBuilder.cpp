//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/MaterialBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <core/file/FileIO.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <render/adaptor/assets/MaterialAsset.h>

namespace sky::builder {

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
                    std::string techPath = am->GetRealPath(tech.GetString());
                    Uuid techId;
                    if (am->QueryOrImportSource(techPath, {TechniqueBuilder::KEY.data()}, techId)) {
                        assetData.techniques.emplace_back(am->LoadAsset<Technique>(techId));
                    }
                }
            }
        }
        result.products.emplace_back(BuildProduct{KEY.data(), asset->GetUuid()});
        am->SaveAsset(asset);
    }
}
