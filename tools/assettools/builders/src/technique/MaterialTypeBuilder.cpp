//
// Created by Zach Lee on 2022/9/19.
//

#include <builders/technique/MaterialTypeBuilder.h>
#include <core/file/FileIO.h>
#include <framework/asset/AssetManager.h>
#include <rapidjson/document.h>
#include <render/resources/Material.h>

namespace sky {

    MaterialTypeBuilder::MaterialTypeBuilder()
    {
    }

    const std::vector<std::string>& MaterialTypeBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".mattype"
        };
        return extensions;
    }

    void MaterialTypeBuilder::Build(const std::string& projectPath, const std::filesystem::path& path) const
    {
        auto fileName = path.filename().replace_extension();

        std::string json;
        ReadString(path.string(), json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("gfxTechniques")) {
            return;
        }

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("techniques");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }

        std::filesystem::path outPath = dataPath;
        std::stringstream ss;
        ss << fileName.string() << ".mtype";
        auto outputPath = outPath.append(ss.str());

        MaterialType data;
        auto asset = std::make_shared<Asset<MaterialType>>();

        auto array = document["gfxTechniques"].GetArray();
        for (auto &value : array) {
            std::filesystem::path path = value.GetString();
            auto relativePath = path.make_preferred().string();
            auto uuid = Uuid::CreateWithSeed(Fnv1a32(relativePath));
            auto techAsset = std::make_shared<Asset<GraphicsTechnique>>();
            techAsset->SetUuid(uuid);
            data.gfxTechniques.emplace_back(techAsset);
            data.assetPathMap.emplace(uuid, relativePath);
        }
        asset->SetData(std::move(data));

        AssetManager::Get()->SaveAsset(asset, outputPath.string());
    }

}
