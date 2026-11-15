#pragma once
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/SceneAssets.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/rhi/VertexSemantic.h>
#include <cstring>
#include <framework/asset/AssetManager.h>
#include <framework/asset/ISourceCatalog.h>

namespace sky::aurora {
    enum class ScenePrimitive { CUBE, PLANE };
    inline Uuid ScenePrimitiveId(ScenePrimitive p)
    {
        return Uuid::CreateWithSeed(p == ScenePrimitive::CUBE ? 8101 : 8102);
    }
    inline Uuid SceneUnlitMaterialId()
    {
        return Uuid::CreateWithSeed(8103);
    }
    inline Uuid SceneLitMaterialId()
    {
        return Uuid::CreateWithSeed(8104);
    }
    inline MeshAssetData ScenePrimitiveMesh(ScenePrimitive primitive)
    {
        MeshAssetData               mesh;
        const std::vector<Vector3>  vertices = primitive == ScenePrimitive::CUBE
                                                   ? std::vector<Vector3>{{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
                                                                          {-.5f, -.5f, .5f},  {.5f, -.5f, .5f},  {.5f, .5f, .5f},  {-.5f, .5f, .5f}}
                                                   : std::vector<Vector3>{{-2, 0, -2}, {2, 0, -2}, {2, 0, 2}, {-2, 0, 2}};
        const std::vector<uint16_t> indices =
            primitive == ScenePrimitive::CUBE
                ? std::vector<uint16_t>{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5}
                : std::vector<uint16_t>{0, 2, 1, 0, 3, 2};
        mesh.vertexCount  = static_cast<uint32_t>(vertices.size());
        mesh.vertexStride = sizeof(Vector3);
        mesh.vertexSize   = vertices.size() * sizeof(Vector3);
        mesh.vertexData.resize(mesh.vertexSize);
        mesh.indexCount = static_cast<uint32_t>(indices.size());
        mesh.indexType  = static_cast<uint32_t>(IndexType::U16);
        mesh.indexSize  = indices.size() * sizeof(uint16_t);
        mesh.indexData.resize(mesh.indexSize);
        std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexSize);
        std::memcpy(mesh.indexData.data(), indices.data(), mesh.indexSize);
        mesh.attributes.push_back({.semantic      = static_cast<uint8_t>(VertexSemantic::POSITION),
                                   .semanticIndex = 0,
                                   .reserved      = 0,
                                   .format        = static_cast<uint32_t>(Format::F_RGB32),
                                   .offset        = 0});
        mesh.subMeshes.push_back({.indexOffset = 0, .indexCount = mesh.indexCount, .materialIndex = 0});
        mesh.bounds    = primitive == ScenePrimitive::CUBE ? BoundingBoxSphere{{0, 0, 0}, {.5f, .5f, .5f}} : BoundingBoxSphere{{0, 0, 0}, {2, 0, 2}};
        mesh.materials = {SceneUnlitMaterialId()};
        return mesh;
    }

    template <class T>
    struct LoadedSceneTestAsset : Asset<T> {
        LoadedSceneTestAsset()
        {
            this->status.store(AssetBase::Status::LOADED);
            this->SetType(Name(AssetTraits<T>::ASSET_TYPE.data()));
        }
    };
    inline AssetPtr SceneFixtureAsset(const Uuid &id)
    {
        if (id == ScenePrimitiveId(ScenePrimitive::CUBE) || id == ScenePrimitiveId(ScenePrimitive::PLANE)) {
            auto asset = std::make_shared<LoadedSceneTestAsset<Mesh>>();
            asset->SetUuid(id);
            asset->Data() = ScenePrimitiveMesh(id == ScenePrimitiveId(ScenePrimitive::CUBE) ? ScenePrimitive::CUBE : ScenePrimitive::PLANE);
            return asset;
        }
        if (id != SceneLitMaterialId() && id != SceneUnlitMaterialId())
            return {};
        auto asset = std::make_shared<LoadedSceneTestAsset<Material>>();
        asset->SetUuid(id);
        MaterialTechniqueData opaque;
        opaque.tag             = "opaque";
        const std::string path = id == SceneLitMaterialId() ? "aurora/materials/Lit.slang" : "aurora/materials/Unlit.slang";
        opaque.program         = {.vertex = {.relativePath = path, .entry = "VSMain"}, .fragment = {.relativePath = path, .entry = "FSMain"}};
        opaque.aliases         = {{"tint", "gMaterial.tint"}, {"surface", "gMaterial.surface"}};
        opaque.renderState.depthStencil.depthTest = opaque.renderState.depthStencil.depthWrite = true;
        opaque.renderState.depthStencil.compareOp                                              = CompareOp::LESS_OR_EQUAL;
        auto shadow                                                                            = opaque;
        shadow.tag                                                                             = "shadow";
        shadow.program = {.vertex   = {.relativePath = "aurora/materials/Shadow.slang", .entry = "VSMain"},
                          .fragment = {.relativePath = "aurora/materials/Shadow.slang", .entry = "FSMain"}};
        shadow.aliases.clear();
        asset->Data().techniques = {opaque, shadow};
        asset->Data().properties = {{"tint", MaterialPropertyType::FLOAT4, Vector4{.65f, .72f, .82f, 1}},
                                    {"surface", MaterialPropertyType::FLOAT4, Vector4{.5f, 0, 0, 0}}};
        return asset;
    }
    inline std::shared_ptr<ISceneResourceSource>
    CreateTestSceneAssetSource(std::shared_ptr<ShaderFileSystem>                    files,
                               std::function<AssetPtr(const Uuid &)>                loader       = {},
                               FileSystemPtr                                        products     = {},
                               std::shared_ptr<const SceneMaterialCapabilitySource> capabilities = {},
                               SceneMaterialCompilation                             compilation  = SceneMaterialCompilation::Synchronous)
    {
        auto surface     = std::make_shared<SceneMaterialCapabilitySource>();
        surface->resolve = [capabilities](const Uuid &id, const MaterialAssetData &data, uint64_t revision) {
            if (id == SceneLitMaterialId() || id == SceneUnlitMaterialId())
                return SceneSurfaceFacts::StaticOpaque(revision);
            return capabilities && capabilities->resolve ? capabilities->resolve(id, data, revision) : SceneSurfaceFacts{};
        };
        return CreateSceneAssetSource(
            std::move(files),
            [loader = std::move(loader)](const Uuid &id) {
                if (auto asset = SceneFixtureAsset(id))
                    return asset;
                return loader ? loader(id) : AssetManager::Get()->LoadAsset(id);
            },
            std::move(products), std::move(surface), compilation);
    }
    class SceneFixtureCatalog final : public ISourceCatalog {
    public:
        bool ResolvePath(const std::string &path, Uuid &out) const override
        {
            if (path == scene_assets::CUBE)
                out = ScenePrimitiveId(ScenePrimitive::CUBE);
            else if (path == scene_assets::PLANE)
                out = ScenePrimitiveId(ScenePrimitive::PLANE);
            else if (path == scene_assets::LIT)
                out = SceneLitMaterialId();
            else if (path == scene_assets::UNLIT)
                out = SceneUnlitMaterialId();
            else
                return false;
            return true;
        }
        bool Exists(const Uuid &) const override
        {
            return false;
        }
        bool GetType(const Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetTarget(const Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetSourcePath(const Uuid &, std::string &) const override
        {
            return false;
        }
    };
} // namespace sky::aurora
