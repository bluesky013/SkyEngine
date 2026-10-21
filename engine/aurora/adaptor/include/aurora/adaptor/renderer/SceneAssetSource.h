#pragma once

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <functional>

namespace sky::aurora {
    enum class ScenePrimitive { CUBE, PLANE };
    Uuid                                               ScenePrimitiveId(ScenePrimitive primitive);
    Uuid                                               SceneLitMaterialId();
    MeshAssetData                                      ScenePrimitiveMesh(ScenePrimitive primitive);
    std::variant<RendererError, RenderResourcePayload> ConvertSceneMesh(const Asset<Mesh> &asset, RendererInfo info);
    std::variant<RendererError, RenderResourcePayload> ConvertSceneImage(const Asset<Texture> &asset, RendererInfo info);

    // The loader returns owned assets; ResolveMesh polls status without waiting on asset workers.
    // Sources are supplied by the host; an empty provider supports cooked-only or non-shader asset requests.
    std::shared_ptr<ISceneResourceSource>
    CreateSceneAssetSource(std::shared_ptr<ShaderFileSystem> files, std::function<AssetPtr(const Uuid &)> loader = {}, FileSystemPtr products = {});
} // namespace sky::aurora
