//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/ShaderBuilder.h>

#include <framework/asset/AssetManager.h>

#include <shader/ShaderCompiler.h>
#include <render/resource/Shader.h>
#include <render/adaptor/assets/ShaderAsset.h>

namespace sky::builder {

    void ShaderBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        auto shader = AssetManager::Get()->CreateAsset<ShaderCollection>(request.uuid);
        shader->Data().shaderSource = sl::ShaderCompiler::Get()->LoadShader(request.relativePath);

        result.products.emplace_back(BuildProduct{"GFX_SHASER", shader});
        result.success = true;
    }

    void ShaderBuilder::LoadConfig(const std::string &path)
    {
    }
}
