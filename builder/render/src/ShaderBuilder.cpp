//
// Created by Zach Lee on 2023/2/23.
//


#include <builder/render/ShaderBuilder.h>
#include <builder/shader/ShaderCompiler.h>
#include <framework/asset/AssetManager.h>

#include <render/assets/Shader.h>
#include <filesystem>

namespace sky::builder {

    void ShaderBuilder::Request(const BuildRequest &request, BuildResult &result)
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
        auto *am = AssetManager::Get();
        std::filesystem::path outDir = request.projectDir + "/shaders";
        std::filesystem::create_directories(outDir);
        std::string fullPath = outDir.append(request.name).make_preferred().string();
        std::string outFullPath = fullPath + ".bin";
        std::string outVariantPath = fullPath + ".variant";
        auto asset = am->CreateAsset<Shader>(outFullPath);
        auto defaultVariant = am->CreateAsset<ShaderVariant>(outVariantPath);

        auto &variantData = defaultVariant->Data();
        // save spv
        ShaderCompiler::BuildSpirV(request.fullPath, type, variantData.spv);
        if (variantData.spv.empty()) {
            return;
        }

        // save gles
        variantData.gles = ShaderCompiler::BuildGLES(variantData.spv);
        asset->Data().variants.emplace("", defaultVariant);

        result.products.emplace_back(BuildProduct{"GFX_SHADER", asset->GetUuid()});
        am->SaveAsset(asset);
        am->SaveAsset(defaultVariant);
    }
}