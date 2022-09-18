//
// Created by Zach Lee on 2022/9/18.
//

#include <builders/technique/TechniqueBuilder.h>
#include <cereal/external/rapidjson/pointer.h>
#include <cereal/external/rapidjson/rapidjson.h>
#include <core/file/FileIO.h>
#include <render/resources/Technique.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    TechniqueBuilder::TechniqueBuilder()
    {

    }

    const std::vector<std::string>& TechniqueBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".technique"
        };
        return extensions;
    }

    static ShaderAssetPtr GetShader(GfxTechniqueAssetData &data, rapidjson::Document &document, const std::string &shaderStage)
    {
        ShaderAssetPtr asset;
        auto &val = document["shaders"];
        if (val.HasMember(shaderStage.c_str())) {
            std::filesystem::path path = val[shaderStage.c_str()].GetString();
            auto relativePath = path.make_preferred().string();
            auto uuid = Uuid::CreateWithSeed(Fnv1a32(relativePath));
            asset = std::make_shared<Asset<Shader>>();
            asset->SetUuid(uuid);
            data.assetPathMap.emplace(uuid, relativePath);
        }
        return asset;
    }

    static void ProcessGraphics(const std::string& dataPath, const std::filesystem::path& path, rapidjson::Document &document)
    {
        auto fileName = path.filename().replace_extension();
        auto asset = std::make_shared<Asset<GraphicsTechnique>>();
        GfxTechniqueAssetData data;

        data.vs = GetShader(data, document, "vert");
        data.fs = GetShader(data, document, "frag");

        if (document.HasMember("depth_stencil")) {
            auto& dsObject = document["depth_stencil"];
            if (dsObject.HasMember("depthTestEnable")) {
                data.depthStencilState.depthTestEnable = dsObject["depthTestEnable"].GetBool();
            }
            if (dsObject.HasMember("depthWriteEnable")) {
                data.depthStencilState.depthTestEnable = dsObject["depthWriteEnable"].GetBool();
            }
        }
        asset->SetData(std::move(data));

        std::filesystem::path outPath = dataPath;
        std::stringstream ss;
        ss << fileName.string() << ".tech";
        auto outputPath = outPath.append(ss.str());
        AssetManager::Get()->SaveAsset(asset, outputPath.string());
    }

    void TechniqueBuilder::Build(const std::string& projectPath, const std::filesystem::path& path) const
    {
        auto fileName = path.filename();

        std::string json;
        ReadString(path.string(), json);

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (!document.HasMember("type")) {
            return;
        }

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("techniques");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }

        std::string type = document["type"].GetString();
        if (type == "graphics") {
            ProcessGraphics(dataPath.string(), path, document);
        }
    }
}