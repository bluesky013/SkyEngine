//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/ShaderBuilder.h>

#include <framework/asset/AssetManager.h>

#include <shader/ShaderCompiler.h>
#include <render/resource/Shader.h>
#include <render/adaptor/assets/ShaderAsset.h>

#include <core/hash/Crc32.h>

namespace sky::builder {

    void ShaderBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        auto shader = AssetManager::Get()->CreateAsset<ShaderCollection>(request.uuid);
        shader->Data().name = request.name;
        shader->Data().shaderSource = ShaderCompiler::Get()->LoadShader(request.relativePath);
        shader->Data().hash = Crc32::Cal(shader->Data().shaderSource);

        result.products.emplace_back(BuildProduct{"GFX_SHASER", shader});
        result.success = true;
    }

    void ShaderBuilder::LoadConfig(const FileSystemPtr &fs, const std::string &path)
    {

    }
}
