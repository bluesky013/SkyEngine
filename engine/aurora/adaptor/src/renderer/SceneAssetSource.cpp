#include <aurora/adaptor/renderer/SceneAssetSource.h>

#include <algorithm>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/pipeline/SceneMaterial.h>
#include <aurora/rhi/VertexSemantic.h>
#include <cmath>
#include <cstring>
#include <framework/asset/AssetManager.h>
#include <map>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        RendererError Invalid(std::string message)
        {
            return {.code = RendererErrorCode::INVALID_ARGUMENT, .message = std::move(message)};
        }
        constexpr uint64_t MAX_MESH_BYTES   = 64ull * 1024 * 1024;
        constexpr auto     DEFAULT_MATERIAL = Uuid::CreateFromString("8F015801-3437-4A44-A581-C191184C8103");
    } // namespace
    Uuid ScenePrimitiveId(ScenePrimitive primitive)
    {
        return primitive == ScenePrimitive::CUBE ? Uuid::CreateFromString("8F015801-3437-4A44-A581-C191184C8101")
                                                 : Uuid::CreateFromString("8F015801-3437-4A44-A581-C191184C8102");
    }
    Uuid SceneLitMaterialId()
    {
        return Uuid::CreateFromString("8F015801-3437-4A44-A581-C191184C8104");
    }
    MeshAssetData ScenePrimitiveMesh(ScenePrimitive primitive)
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
        mesh.bounds = primitive == ScenePrimitive::CUBE ? BoundingBoxSphere{{0, 0, 0}, {.5f, .5f, .5f}} : BoundingBoxSphere{{0, 0, 0}, {2, 0, 2}};
        return mesh;
    }
    std::variant<RendererError, RenderResourcePayload> ConvertSceneMesh(const Asset<Mesh> &asset, RendererInfo info)
    {
        const auto    &data      = asset.Data();
        const auto     indexType = static_cast<IndexType>(data.indexType);
        const uint32_t indexSize = indexType == IndexType::U16 ? 2 : 4;
        if (!asset.GetUuid() || (info.deviceEpoch == 0u) || data.version != MeshAssetData::CURRENT_VERSION || (data.vertexCount == 0u) ||
            (data.vertexStride == 0u) || (data.indexCount == 0u) || data.vertexSize > MAX_MESH_BYTES || data.indexSize > MAX_MESH_BYTES ||
            data.vertexSize + data.indexSize > MAX_MESH_BYTES || data.vertexSize != uint64_t(data.vertexCount) * data.vertexStride ||
            (indexType != IndexType::U16 && indexType != IndexType::U32) || data.indexSize != uint64_t(data.indexCount) * indexSize ||
            data.attributes.empty() || data.attributes.size() > 16 || data.subMeshes.size() > 8192 || (data.skinSize != 0u) || data.skeleton) {
            return Invalid("Static scene mesh has invalid ranges or unsupported skin data");
        }
        auto vertices = data.vertexData.empty() ? asset.ReadBulk(data.vertexOffset, data.vertexSize) : data.vertexData;
        auto indices  = data.indexData.empty() ? asset.ReadBulk(data.indexOffset, data.indexSize) : data.indexData;
        if (vertices.size() != data.vertexSize || indices.size() != data.indexSize) {
            return Invalid("Scene mesh bulk read is incomplete");
        }
        RenderResourcePayload result;
        result.resource        = {.product = asset.GetUuid(), .revision = 1, .usageVariant = 1, .deviceEpoch = info.deviceEpoch};
        result.format          = Name("runtime-mesh");
        result.metadata.bounds = data.bounds.ToAABB();
        if (!IsValidRenderResourceMetadata(result.metadata)) {
            return Invalid("Scene mesh bounds are invalid");
        }
        auto &mesh = result.mesh.emplace();
        mesh.streams.push_back({.offset = 0, .size = data.vertexSize, .stride = data.vertexStride});
        std::set<std::pair<uint8_t, uint8_t>> semantics;
        bool                                  position = false;
        for (const auto &attribute : data.attributes) {
            const auto format = static_cast<Format>(attribute.format);
            uint32_t   size   = 0;
            switch (format) {
            case Format::F_R32: size = 4; break;
            case Format::F_RG32: size = 8; break;
            case Format::F_RGB32: size = 12; break;
            case Format::F_RGBA32: size = 16; break;
            default: return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Scene mesh vertex format is unsupported"};
            }
            if (attribute.semantic >= kVertexSemanticCount || attribute.offset > data.vertexStride || size > data.vertexStride - attribute.offset ||
                !semantics.emplace(attribute.semantic, attribute.semanticIndex).second) {
                return Invalid("Scene mesh vertex attributes are invalid");
            }
            const auto semantic = static_cast<VertexSemantic>(attribute.semantic);
            position |= semantic == VertexSemantic::POSITION && attribute.semanticIndex == 0 && format == Format::F_RGB32;
            mesh.attributes.push_back({.location      = static_cast<uint32_t>(mesh.attributes.size()),
                                       .binding       = 0,
                                       .offset        = attribute.offset,
                                       .format        = format,
                                       .semantic      = VertexSemanticName(semantic),
                                       .semanticIndex = attribute.semanticIndex});
        }
        if (!position) {
            return Invalid("Scene mesh requires float3 POSITION");
        }
        for (uint32_t i = 0; i < data.indexCount; ++i) {
            uint32_t index = 0;
            std::memcpy(&index, indices.data() + (uint64_t(i) * indexSize), indexSize);
            if (index >= data.vertexCount) {
                return Invalid("Scene mesh index exceeds its vertex range");
            }
        }
        const auto indexOffset = (vertices.size() + 3ull) & ~3ull;
        result.bytes           = std::move(vertices);
        result.bytes.resize(indexOffset);
        result.bytes.insert(result.bytes.end(), indices.begin(), indices.end());
        RenderMeshLod lod{.offset = indexOffset, .size = data.indexSize, .type = indexType, .command = {.indexCount = data.indexCount}};
        for (const auto &section : data.subMeshes) {
            if ((section.indexCount == 0u) || section.indexOffset > data.indexCount || section.indexCount > data.indexCount - section.indexOffset) {
                return Invalid("Scene mesh section exceeds its index range");
            }
            lod.sections.push_back({.indexCount = section.indexCount, .instanceCount = 1, .firstIndex = section.indexOffset});
        }
        mesh.lods.push_back(std::move(lod));
        return result;
    }

    namespace {
        class SceneAssetSource final : public ISceneResourceSource {
        public:
            SceneAssetSource(std::shared_ptr<ShaderFileSystem> source, std::function<AssetPtr(const Uuid &)> load)
                : files(std::move(source)), loader(std::move(load))
            {
                RegisterSceneShaderSources(*files);
            }
            SceneResourceResult ResolveMesh(const Uuid &id, const Uuid &overrideMaterial, RendererInfo info) override
            {
                const auto key = std::tuple{id, overrideMaterial, info.backend, info.deviceEpoch};
                if (const auto found = cache.find(key); found != cache.end()) {
                    return found->second;
                }
                if (cache.size() >= 128 || assets.size() >= 1024) {
                    return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene asset cache budget exceeded"};
                }
                auto asset = Load(id);
                if (!asset) {
                    return Invalid("Scene mesh asset could not be loaded");
                }
                if (!(asset->GetType() == AssetTraits<Mesh>::ASSET_TYPE)) {
                    return Invalid("Scene mesh asset type mismatch");
                }
                if (asset->GetStatus() == AssetBase::Status::FAILED) {
                    return Invalid("Scene mesh asset load failed");
                }
                const bool primitive = id == ScenePrimitiveId(ScenePrimitive::CUBE) || id == ScenePrimitiveId(ScenePrimitive::PLANE);
                if (!primitive && !asset->IsLoaded()) {
                    return {};
                }
                const auto meshAsset = std::static_pointer_cast<Asset<Mesh>>(asset);
                auto       converted = ConvertSceneMesh(*meshAsset, info);
                if (const auto *error = std::get_if<RendererError>(&converted)) {
                    return *error;
                }
                auto result = std::make_shared<SceneMeshResources>();
                result->payloads.push_back(std::move(std::get<RenderResourcePayload>(converted)));
                const auto                        meshKey = result->payloads.front().resource;
                const auto                       &data    = meshAsset->Data();
                const size_t                      count   = std::max(size_t{1}, data.subMeshes.size());
                std::map<Uuid, RenderResourceKey> materials;
                for (uint32_t part = 0; part < count; ++part) {
                    const auto slot = data.subMeshes.empty() ? 0 : data.subMeshes[part].materialIndex;
                    if (!data.materials.empty() && slot >= data.materials.size()) {
                        return Invalid("Scene mesh material slot exceeds its table");
                    }
                    auto materialId = DEFAULT_MATERIAL;
                    if (slot == 0 && overrideMaterial) {
                        materialId = overrideMaterial;
                    } else if (!data.materials.empty() && data.materials[slot]) {
                        materialId = data.materials[slot];
                    }
                    if (!materials.contains(materialId)) {
                        const MaterialAssetData *material = nullptr;
                        AssetPtr                 loaded;
                        if (materialId != DEFAULT_MATERIAL && materialId != SceneLitMaterialId()) {
                            loaded = Load(materialId);
                            if (!loaded || !(loaded->GetType() == AssetTraits<Material>::ASSET_TYPE)) {
                                return Invalid("Scene material asset type mismatch");
                            }
                            if (loaded->GetStatus() == AssetBase::Status::FAILED) {
                                return Invalid("Scene material asset load failed");
                            }
                            if (!loaded->IsLoaded()) {
                                return {};
                            }
                            material = &std::static_pointer_cast<Asset<Material>>(loaded)->Data();
                        }
                        auto ready = MaterialPayload(materialId, material, info);
                        if (const auto *error = std::get_if<RendererError>(&ready)) {
                            return *error;
                        }
                        auto payload = std::move(std::get<RenderResourcePayload>(ready));
                        materials.emplace(materialId, payload.resource);
                        result->payloads.push_back(std::move(payload));
                    }
                    result->parts.push_back({.part     = part,
                                             .mesh     = {.resource = meshKey, .bindingRevision = 1},
                                             .material = {.resource = materials.at(materialId), .bindingRevision = 1}});
                }
                uint64_t bytes = 0;
                for (const auto &payload : result->payloads) {
                    bytes += RenderResourceCpuBytes(payload);
                }
                if (bytes > 256ull * 1024 * 1024 - cachedBytes) {
                    return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene asset CPU byte budget exceeded"};
                }
                cachedBytes += bytes;
                cache.emplace(key, result);
                return std::shared_ptr<const SceneMeshResources>(std::move(result));
            }
            SceneImageResult ResolveImage(const Uuid &id, RendererInfo info) override
            {
                const auto key = std::pair{id, info.deviceEpoch};
                if (const auto found = imageCache.find(key); found != imageCache.end()) {
                    return found->second;
                }
                if (imageCache.size() >= 128 || assets.size() >= 1024) {
                    return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Environment asset cache budget exceeded"};
                }
                const auto asset = Load(id);
                if (!asset || !(asset->GetType() == AssetTraits<Texture>::ASSET_TYPE)) {
                    return Invalid("Environment image asset type mismatch or missing asset");
                }
                if (asset->GetStatus() == AssetBase::Status::FAILED) {
                    return Invalid("Environment image asset load failed");
                }
                if (!asset->IsLoaded()) {
                    return {};
                }
                auto converted = ConvertSceneImage(*std::static_pointer_cast<Asset<Texture>>(asset), info);
                if (const auto *error = std::get_if<RendererError>(&converted)) {
                    return *error;
                }
                auto       result = std::make_shared<const RenderResourcePayload>(std::move(std::get<RenderResourcePayload>(converted)));
                const auto bytes  = RenderResourceCpuBytes(*result);
                if (bytes > 256ull * 1024 * 1024 - cachedBytes) {
                    return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene asset CPU byte budget exceeded"};
                }
                cachedBytes += bytes;
                imageCache.emplace(key, result);
                return result;
            }

        private:
            AssetPtr Load(const Uuid &id)
            {
                if (const auto found = assets.find(id); found != assets.end()) {
                    return found->second;
                }
                AssetPtr result;
                if (id == ScenePrimitiveId(ScenePrimitive::CUBE) || id == ScenePrimitiveId(ScenePrimitive::PLANE)) {
                    auto mesh = std::make_shared<Asset<Mesh>>();
                    mesh->SetUuid(id);
                    mesh->SetType(Name(AssetTraits<Mesh>::ASSET_TYPE.data()));
                    mesh->Data() = ScenePrimitiveMesh(id == ScenePrimitiveId(ScenePrimitive::CUBE) ? ScenePrimitive::CUBE : ScenePrimitive::PLANE);
                    result       = std::move(mesh);
                } else {
                    result = loader(id);
                }
                if (result) {
                    assets.emplace(id, result);
                }
                return result;
            }
            std::variant<RendererError, RenderResourcePayload> MaterialPayload(const Uuid &id, const MaterialAssetData *data, RendererInfo info)
            {
                Vector4   tint{.65f, .72f, .82f, 1};
                ShaderRef shader;
                if (id == SceneLitMaterialId()) {
                    shader.relativePath = "SceneLit.slang";
                }
                if (data != nullptr) {
                    if (data->version != MaterialAssetData::CURRENT_VERSION || data->techniques.size() > 1) {
                        return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Scene material requires one scene technique"};
                    }
                    if (!data->techniques.empty()) {
                        shader = data->techniques.front().shader;
                    }
                    for (const auto &property : data->properties) {
                        const auto *value = std::get_if<Vector4>(&property.value);
                        if (property.name != "tint" || property.type != MaterialPropertyType::FLOAT4 || (value == nullptr)) {
                            return RendererError{.code    = RendererErrorCode::UNSUPPORTED,
                                                 .message = "Scene material property is not in SceneBatchParams"};
                        }
                        tint = *value;
                    }
                }
                if (!std::isfinite(tint.x) || !std::isfinite(tint.y) || !std::isfinite(tint.z) || !std::isfinite(tint.w)) {
                    return Invalid("Scene material tint is not finite");
                }
                auto compiled = CompileSceneMaterial(*files, info.backend, shader);
                if (const auto *error = std::get_if<RendererError>(&compiled)) {
                    return *error;
                }
                RenderResourcePayload result;
                result.format   = Name("runtime-material");
                result.bytes    = SceneBatchBytes(tint);
                result.material = std::move(std::get<RenderMaterialData>(compiled));
                auto &material  = *result.material;
                material.depth  = {.test = true, .write = true, .compare = CompareOp::LESS_OR_EQUAL};
                if ((data != nullptr) && !data->techniques.empty()) {
                    const auto &state = data->techniques.front().renderState;
                    material.depth    = {.test    = bool(state.depthStencil.depthTest),
                                         .write   = bool(state.depthStencil.depthWrite),
                                         .compare = state.depthStencil.compareOp};
                    if (state.blendStates.size() > 1) {
                        return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Scene material has multiple blend targets"};
                    }
                    if (!state.blendStates.empty()) {
                        material.blend = state.blendStates.front();
                    }
                }
                // This source freezes assets for its lifetime; revisions must be ordered, not content hashes.
                result.resource = {.product = id, .revision = 1, .usageVariant = 1, .deviceEpoch = info.deviceEpoch};
                return result;
            }
            std::shared_ptr<ShaderFileSystem>                                                          files;
            std::function<AssetPtr(const Uuid &)>                                                      loader;
            std::map<Uuid, AssetPtr>                                                                   assets;
            std::map<std::tuple<Uuid, Uuid, API, uint64_t>, std::shared_ptr<const SceneMeshResources>> cache;
            std::map<std::pair<Uuid, uint64_t>, std::shared_ptr<const RenderResourcePayload>>          imageCache;
            uint64_t                                                                                   cachedBytes = 0;
        };
    } // namespace
    std::shared_ptr<ISceneResourceSource> CreateSceneAssetSource(std::shared_ptr<ShaderFileSystem>     files,
                                                                 std::function<AssetPtr(const Uuid &)> loader)
    {
        if (!files) {
            files = std::make_shared<ShaderFileSystem>();
        }
        if (!loader) {
            loader = [](const Uuid &id) { return AssetManager::Get()->LoadAsset(id); };
        }
        return std::make_shared<SceneAssetSource>(std::move(files), std::move(loader));
    }
} // namespace sky::aurora
