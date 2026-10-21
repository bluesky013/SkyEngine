#include <aurora/pipeline/SceneMaterial.h>

#include "SceneBatch.h"
#include "SceneShaderSources.h"
#include <algorithm>
#include <aurora/renderer/RenderCommands.h>
#include <aurora/shader/ShaderResolver.h>
#include <cstring>

namespace sky::aurora {
    void RegisterSceneShaderSources(ShaderFileSystem &files)
    {
        for (const auto &source : generated::sceneShaderSources) {
            files.AddVirtualFile(source.path, source.text);
        }
    }
    std::vector<uint8_t> SceneBatchBytes(const Vector4 &tint)
    {
        const shared::SceneBatchParams batch{.tint = tint, .world = RenderIdentityMatrix()};
        std::vector<uint8_t>           bytes(sizeof(batch));
        std::memcpy(bytes.data(), &batch, sizeof(batch));
        return bytes;
    }
    std::variant<RendererError, RenderMaterialData> CompileSceneMaterial(ShaderFileSystem &files, API backend, const ShaderRef &shader)
    {
        if (backend != API::VULKAN && backend != API::DX12 && backend != API::METAL)
            return RendererError{RendererErrorCode::UNSUPPORTED, "Scene shader backend is unsupported"};
        RenderMaterialData result;
        result.backend       = backend;
        result.vertexEntry   = shader.entry.empty() ? "VSMain" : shader.entry + "VS";
        result.fragmentEntry = shader.entry.empty() ? "FSMain" : shader.entry + "FS";
        ShaderResolver            resolver(nullptr, nullptr, files, 0, 0);
        const ShaderVariantSchema builtinSchema;
        for (const bool vertex : {true, false}) {
            ShaderResolver::Request request;
            request.relativePath = shader.relativePath.empty() ? "SceneForward.slang" : shader.relativePath;
            if (shader.relativePath.empty() || shader.relativePath == "SceneForward.slang" || shader.relativePath == "SceneLit.slang" ||
                shader.relativePath == "SceneShadow.slang" || shader.relativePath == "SceneToneMap.slang")
                request.schema = &builtinSchema;
            request.entry  = vertex ? result.vertexEntry : result.fragmentEntry;
            request.stage  = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            request.target = static_cast<uint32_t>(backend == API::DX12    ? ShaderTarget::DXIL
                                                   : backend == API::METAL ? ShaderTarget::MSL
                                                                           : ShaderTarget::SPIRV);
            ShaderCompileResult compiled;
            std::string         error;
            if (!resolver.ResolveShader(request, compiled, &error))
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene shader compile failed: " + error};
            auto &code = vertex ? result.vertexCode : result.fragmentCode;
            code.resize(compiled.data.size() * sizeof(uint32_t));
            std::memcpy(code.data(), compiled.data.data(), code.size());
            for (const auto &resource : compiled.reflection.resources) {
                const auto found = std::find_if(result.reflection.resources.begin(), result.reflection.resources.end(),
                                                [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
                if (found == result.reflection.resources.end())
                    result.reflection.resources.push_back(resource);
                else if (found->type != resource.type || found->count != resource.count)
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene shader stages disagree on resource bindings"};
            }
            for (const auto &block : compiled.reflection.blocks) {
                const auto found = std::find_if(result.reflection.blocks.begin(), result.reflection.blocks.end(),
                                                [&](const auto &item) { return item.set == block.set && item.binding == block.binding; });
                if (found == result.reflection.blocks.end())
                    result.reflection.blocks.push_back(block);
                else if (found->size != block.size || found->members != block.members)
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene shader stages disagree on uniform layout"};
            }
        }
        return result;
    }
} // namespace sky::aurora
