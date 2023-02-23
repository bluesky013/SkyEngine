//
// Created by Zach Lee on 2023/2/23.
//


#include <builder/render/ShaderBuilder.h>
#include <builder/shader/ShaderCompiler.h>
#include <framework/asset/AssetManager.h>

#include <render/assets/Shader.h>
#include <filesystem>

namespace sky::builder {

    void ShaderBuilder::Request(BuildRequest &request)
    {
        ShaderType type;
        std::string outExt;
        if (request.ext == ".vert") {
            type = ShaderType::VS;
        } else if (request.ext == ".frag") {
            type = ShaderType::FS;
        } else if (request.ext == ".comp") {
            type = ShaderType::CS;
        } else {
            return;
        }

        std::vector<uint32_t> spvData;
        ShaderCompiler::BuildSpirV(request.fullPath, type, spvData);
        if (spvData.empty()) {
            return;
        }

        auto *am = AssetManager::Get();

        std::string outDir = request.projectDir + "/shaders";
        std::filesystem::create_directories(outDir);
        // save spv
        {
            std::filesystem::path outPath(outDir);
            outPath.append(request.name + ".spv");
            std::string outFullPath = outPath.make_preferred().string();

            auto asset = am->CreateAsset<Shader>(outFullPath);
            asset->Data().data = spvData;
            request.products.emplace_back(BuildProduct{asset->GetUuid()});
            am->SaveAsset(asset);
        }

        // save gles
        {
            std::string glslSrc = ShaderCompiler::BuildGLES(spvData);

            std::filesystem::path outPath(outDir);
            outPath.append(request.name + ".gles");
            std::string outFullPath = outPath.make_preferred().string();

            auto asset = am->CreateAsset<Shader>(outFullPath);
            asset->Data().data.resize(glslSrc.size());
            memcpy(asset->Data().data.data(), glslSrc.data(), glslSrc.size());

            request.products.emplace_back(BuildProduct{asset->GetUuid()});
            am->SaveAsset(asset);
        }
    }
}