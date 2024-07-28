//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/ShaderBuilder.h>

#include <framework/asset/AssetManager.h>
#include <core/hash/Crc32.h>

#include <shader/ShaderCompiler.h>
#include <render/resource/Shader.h>

namespace sky::builder {

    void ShaderBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto shader = AssetManager::Get()->FindOrCreateAsset<ShaderCollection>(request.assetInfo->uuid);
        shader->Data().name = request.assetInfo->name;
        shader->Data().shaderSource = ShaderCompiler::Get()->LoadShader(request.assetInfo->path.path.GetStr());
        shader->Data().hash = Crc32::Cal(shader->Data().shaderSource);

        AssetManager::Get()->SaveAsset(shader, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

    void ShaderBuilder::LoadConfig(const FileSystemPtr &cfg)
    {

    }
}
