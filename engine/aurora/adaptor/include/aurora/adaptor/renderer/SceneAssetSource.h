#pragma once

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <functional>

namespace sky::aurora {
    struct MaterialAssetData;
    struct SceneMaterialCapabilitySource {
        std::function<SceneSurfaceFacts(const Uuid &, const MaterialAssetData &, uint64_t)> resolve;
    };
    std::variant<RendererError, RenderResourcePayload> ConvertSceneMesh(const Asset<Mesh> &asset, RendererInfo info);
    enum class SceneMaterialCompilation { Synchronous, Asynchronous };
    std::variant<RendererError, RenderResourcePayload>
    ConvertSceneImage(const Asset<Texture> &asset, RendererInfo info, SceneImageUsage usage = SceneImageUsage::ENVIRONMENT);

    // The loader returns owned assets; resolution polls status without waiting on asset workers.
    // Sources are supplied by the host; an empty provider supports cooked-only or non-shader asset requests.
    std::shared_ptr<ISceneResourceSource> CreateSceneAssetSource(std::shared_ptr<ShaderFileSystem>                    files,
                                                                 std::function<AssetPtr(const Uuid &)>                loader       = {},
                                                                 FileSystemPtr                                        products     = {},
                                                                 std::shared_ptr<const SceneMaterialCapabilitySource> capabilities = {},
                                                                 SceneMaterialCompilation compilation = SceneMaterialCompilation::Synchronous);
} // namespace sky::aurora
