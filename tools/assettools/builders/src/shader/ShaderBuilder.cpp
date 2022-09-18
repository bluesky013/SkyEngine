//
// Created by Zach Lee on 2022/9/14.
//

#include <core/file/FileIO.h>
#include <builders/shader/ShaderBuilder.h>
#include <framework/asset/AssetManager.h>
#include <render/resources/Shader.h>

namespace sky {

    ShaderBuilder::ShaderBuilder()
    {

    }

    const std::vector<std::string>& ShaderBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".spv"
        };
        return extensions;
    }

    void ShaderBuilder::Build(const std::string& projectPath, const std::filesystem::path& path) const
    {
        auto fileName = path.filename();
        auto shaderType = fileName.replace_extension().extension().string();

        auto shader = std::make_shared<Asset<Shader>>();
        ShaderAssetData assetData = {};

        if (shaderType == "vert") {
            assetData.stage = VK_SHADER_STAGE_VERTEX_BIT;
        } else if (shaderType == "frag") {
            assetData.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        ReadBin(path.string(), assetData.spv);
        shader->SetData(std::move(assetData));

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("shaders");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }

        std::filesystem::path outPath = dataPath;
        std::stringstream ss;
        ss << fileName.string() << ".shader";

        outPath.append(ss.str());
        auto relativePath = std::filesystem::relative(outPath, projectPath).make_preferred().string();
        auto uuid = Uuid::CreateWithSeed(Fnv1a32(relativePath.data()));
        shader->SetUuid(uuid);
        AssetManager::Get()->SaveAsset(shader, outPath.string());
    }

}