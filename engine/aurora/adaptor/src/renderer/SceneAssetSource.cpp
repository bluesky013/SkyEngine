#include <aurora/adaptor/renderer/SceneAssetSource.h>

#include "MaterialBlock.h"
#include "SkinningBatch.h"
#include <algorithm>
#include <atomic>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/pipeline/GlobalVariantLayout.h>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <aurora/pipeline/ShaderVariantAssembler.h>
#include <aurora/rhi/ShaderParameters.h>
#include <aurora/rhi/VertexSemantic.h>
#include <aurora/shader/ShaderCompanion.h>
#include <aurora/shader/ShaderHash.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cmath>
#include <cstring>
#include <framework/asset/AssetEvent.h>
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
    bool IsBuiltinSceneAsset(const Uuid &id, std::string_view type)
    {
        return (type == "AuroraMesh" && (id == ScenePrimitiveId(ScenePrimitive::CUBE) || id == ScenePrimitiveId(ScenePrimitive::PLANE))) ||
               (type == "AuroraMaterial" && (id == DEFAULT_MATERIAL || id == SceneLitMaterialId()));
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
        const auto &data = asset.Data();
        if (data.HasSkinInputs()) {
            const auto status = data.Validate();
            if (!status) {
                return Invalid(status.message);
            }
        }
        const auto     indexType = static_cast<IndexType>(data.indexType);
        const uint32_t indexSize = indexType == IndexType::U16 ? 2 : 4;
        if (!asset.GetUuid() || (info.deviceEpoch == 0u) || data.version != MeshAssetData::CURRENT_VERSION || (data.vertexCount == 0u) ||
            (data.vertexStride == 0u) || (data.indexCount == 0u) || data.vertexSize > MAX_MESH_BYTES || data.indexSize > MAX_MESH_BYTES ||
            data.vertexSize + data.indexSize > MAX_MESH_BYTES || data.vertexSize != uint64_t(data.vertexCount) * data.vertexStride ||
            (indexType != IndexType::U16 && indexType != IndexType::U32) || data.indexSize != uint64_t(data.indexCount) * indexSize ||
            data.attributes.empty() || data.attributes.size() > 16 || data.subMeshes.size() > 8192) {
            return Invalid("Static scene mesh has invalid ranges");
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
            case Format::U_RGBA8: size = 4; break;
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
        if (data.HasSkinInputs()) {
            const auto joints    = std::ranges::find(data.attributes, uint8_t(VertexSemantic::JOINTS), &MeshVertexAttributeData::semantic);
            const auto weights   = std::ranges::find(data.attributes, uint8_t(VertexSemantic::WEIGHTS), &MeshVertexAttributeData::semantic);
            const auto positions = std::ranges::find(data.attributes, uint8_t(VertexSemantic::POSITION), &MeshVertexAttributeData::semantic);
            if (joints->semanticIndex || weights->semanticIndex || positions->semanticIndex) {
                return Invalid("Skin vertex inputs require semantic index zero");
            }
            for (uint32_t v = 0; v < data.vertexCount; ++v) {
                const auto *vertex = vertices.data() + size_t(v) * data.vertexStride;
                for (const auto &attribute : data.attributes) {
                    if (attribute.format == uint32_t(Format::U_RGBA8))
                        continue;
                    const auto components = attribute.format;
                    for (uint32_t component = 0; component < components; ++component) {
                        float value;
                        std::memcpy(&value, vertex + attribute.offset + component * sizeof(float), sizeof(value));
                        if (!std::isfinite(value))
                            return Invalid("Skin vertex attribute is not finite");
                    }
                }
                float   sum = 0;
                Vector3 point;
                std::memcpy(&point, vertex + positions->offset, sizeof(point));
                for (uint32_t axis = 0; axis < 3; ++axis) {
                    if (!std::isfinite(point[axis])) {
                        return Invalid("Skin position is not finite");
                    }
                }
                for (uint32_t influence = 0; influence < 4; ++influence) {
                    float weight;
                    std::memcpy(&weight, vertex + weights->offset + influence * sizeof(float), sizeof(float));
                    const uint8_t slot = vertex[joints->offset + influence];
                    if (!std::isfinite(weight) || weight < 0 || slot >= data.skinInput.slots.size()) {
                        return Invalid("Skin influence has invalid weight or slot");
                    }
                    if (weight > 0) {
                        const auto &bounds = data.slotBounds[slot];
                        if (!bounds.occupied) {
                            return Invalid("Skin positive influence has no slot bounds");
                        }
                        for (uint32_t axis = 0; axis < 3; ++axis) {
                            const float tolerance = 1e-5f * std::max(1.f, std::abs(point[axis]));
                            if (point[axis] < bounds.bounds.min[axis] - tolerance || point[axis] > bounds.bounds.max[axis] + tolerance) {
                                return Invalid("Skin slot bounds do not contain the influenced vertex");
                            }
                        }
                    }
                    sum += weight;
                }
                if (!std::isfinite(sum) || std::abs(sum - 1.f) > 1e-4f) {
                    return Invalid("Skin weights must be normalized");
                }
            }
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
        // Buffer uploads require four-byte size alignment, including odd U16 index counts.
        result.bytes.resize((result.bytes.size() + 3ull) & ~3ull);
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
            struct AssetWatch final : IAssetEvent {
                AssetWatch(Uuid id, SceneAssetSource &source) : id(id), source(source)
                {
                    AsseEvent::Connect(id, this);
                }
                ~AssetWatch()
                {
                    AsseEvent::DisConnect(id, this);
                }
                void OnAssetReload() override
                {
                    ++assetRevision;
                    ++source.revision;
                }
                void OnAssetReset() override
                {
                    ++assetRevision;
                    ++source.revision;
                }
                std::atomic<uint64_t> assetRevision{1};
                Uuid                  id;
                SceneAssetSource     &source;
            };

        public:
            uint64_t Revision() const override
            {
                return revision.load() + AssetManager::Get()->GetProductChangeSerial();
            }
            bool Invalidate(const Uuid &id) override
            {
                if (const auto found = watches.find(id); found != watches.end())
                    ++found->second->assetRevision;
                ++revision;
                return true;
            }
            std::optional<AABB> MeshBounds(const Uuid &id) const override
            {
                const auto found = bounds.find(id);
                return found == bounds.end() ? std::optional<AABB>{} : found->second;
            }
            void Refresh()
            {
                const auto current = Revision();
                if (current == loadedRevision)
                    return;
                cache.clear();
                skeletalCache.clear();
                skinBindings.PruneExpired();
                imageCache.clear();
                assets.clear();
                bounds.clear();
                cachedBytes    = 0;
                loadedRevision = current;
            }
            SceneAssetSource(std::shared_ptr<ShaderFileSystem>                    source,
                             std::function<AssetPtr(const Uuid &)>                load,
                             FileSystemPtr                                        products,
                             std::shared_ptr<const SceneMaterialCapabilitySource> capabilities)
                : files(std::move(source)), loader(std::move(load)), products(std::move(products)), capabilities(std::move(capabilities))
            {
            }
            SceneResourceResult ResolveMesh(const Uuid &id, const Uuid &overrideMaterial, RendererInfo info) override
            {
                return ResolveGeometry(id, overrideMaterial, info, false);
            }
            SceneSkeletalResult
            ResolveSkeletal(const Uuid &id, const Uuid &material, const Uuid &skinId, const Uuid &rigId, RendererInfo info) override
            {
                Refresh();
                const auto key = std::tuple{id, material, skinId, rigId, info.backend, info.deviceEpoch};
                if (const auto found = skeletalCache.find(key); found != skeletalCache.end()) {
                    return found->second;
                }
                if (skeletalCache.size() >= 128) {
                    return RendererError{RendererErrorCode::OUT_OF_MEMORY, "Scene skeletal binding budget exceeded"};
                }
                const auto geometry = ResolveGeometry(id, material, info, true);
                if (const auto *error = std::get_if<RendererError>(&geometry)) {
                    return *error;
                }
                const auto *ready = std::get_if<std::shared_ptr<const SceneMeshResources>>(&geometry);
                if (!ready || !*ready) {
                    return {};
                }
                const auto skinAsset = Load(skinId);
                const auto rigAsset  = Load(rigId);
                if (!skinAsset || !rigAsset || skinAsset->GetType() != Name("AuroraSkin") || rigAsset->GetType() != Name("AuroraSkeleton")) {
                    return Invalid("Skeletal mesh requires explicit Skin and Skeleton assets of the correct type");
                }
                if (skinAsset->GetStatus() == AssetBase::Status::FAILED || rigAsset->GetStatus() == AssetBase::Status::FAILED) {
                    return Invalid("Skeletal mesh Skin or Skeleton failed to load");
                }
                if (!skinAsset->IsLoaded() || !rigAsset->IsLoaded()) {
                    return {};
                }
                const auto                    mesh = std::static_pointer_cast<Asset<Mesh>>(assets.at(id));
                const std::array<AssetPtr, 3> tuple{mesh, skinAsset, rigAsset};
                for (size_t a = 0; a < tuple.size(); ++a) {
                    for (size_t b = a + 1; b < tuple.size(); ++b) {
                        const auto &first  = tuple[a]->GetProductGroup();
                        const auto &second = tuple[b]->GetProductGroup();
                        if (first && second && first->owner == second->owner && first->generation != second->generation) {
                            ++revision;
                            return {};
                        }
                    }
                }
                SkinStatus skinStatus;
                const auto skin = FetchOrCreateSkinByAsset(std::static_pointer_cast<Asset<Skin>>(skinAsset), &skinStatus);
                if (!skin) {
                    return Invalid(skinStatus.message);
                }
                AnimationStatus rigStatus;
                const auto      rig = FetchOrCreateSkeletonByAsset(std::static_pointer_cast<Asset<Skeleton>>(rigAsset), &rigStatus);
                if (!rig) {
                    return Invalid(rigStatus.message);
                }
                SkinTargetLayout target;
                skinStatus = MakeSkinTargetLayout(*rig, target);
                if (!skinStatus) {
                    return Invalid(skinStatus.message);
                }
                const auto    &data   = mesh->Data();
                auto           result = std::make_shared<SceneSkeletalResources>();
                SkinBindingKey bindingKey{id,
                                          skinId,
                                          rigId,
                                          mesh->GetSnapshotRevision(),
                                          skinAsset->GetSnapshotRevision(),
                                          rigAsset->GetSnapshotRevision(),
                                          target.signature,
                                          mesh->GetSnapshotGeneration(),
                                          skinAsset->GetSnapshotGeneration(),
                                          rigAsset->GetSnapshotGeneration()};
                skinStatus = skinBindings.Resolve(data.skinInput, *skin, target, bindingKey, result->binding);
                if (!skinStatus) {
                    return Invalid(skinStatus.message);
                }
                result->geometry        = *ready;
                result->skeleton        = {rigId, bindingKey.skeletonGeneration, bindingKey.skeletonRevision, target.signature};
                result->referenceModels = std::move(target.referenceModels);
                result->slotBounds      = data.slotBounds;
                if (Revision() != loadedRevision)
                    return {};
                skeletalCache.emplace(key, result);
                return std::shared_ptr<const SceneSkeletalResources>(std::move(result));
            }
            SceneResourceResult ResolveGeometry(const Uuid &id, const Uuid &overrideMaterial, RendererInfo info, bool skeletal)
            {
                Refresh();
                const auto key = std::tuple{id, overrideMaterial, info.backend, info.deviceEpoch, skeletal};
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
                if (meshAsset->Data().HasSkinInputs() != skeletal) {
                    return RendererError{RendererErrorCode::UNSUPPORTED, "Mesh influence layout does not match the component pose mode"};
                }
                bounds[id]     = meshAsset->Data().bounds.ToAABB();
                auto converted = ConvertSceneMesh(*meshAsset, info);
                if (const auto *error = std::get_if<RendererError>(&converted)) {
                    return *error;
                }
                auto result                                                  = std::make_shared<SceneMeshResources>();
                std::get<RenderResourcePayload>(converted).resource.revision = meshAsset->GetSnapshotRevision();
                result->payloads.push_back(std::move(std::get<RenderResourcePayload>(converted)));
                const auto                        meshKey = result->payloads.front().resource;
                const auto                       &data    = meshAsset->Data();
                const size_t                      count   = std::max(size_t{1}, data.subMeshes.size());
                std::map<Uuid, RenderResourceKey> materials;
                std::map<Uuid, SceneSurfaceFacts> surfaces;
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
                        MaterialAssetData        instanceMaterial;
                        std::set<std::string>    instanceOverrides;
                        if (materialId != DEFAULT_MATERIAL && materialId != SceneLitMaterialId()) {
                            loaded = Load(materialId);
                            if (!loaded || loaded->GetStatus() == AssetBase::Status::FAILED)
                                return Invalid("Scene material asset is missing or failed");
                            if (!loaded->IsLoaded())
                                return {};
                            if (loaded->GetType() == AssetTraits<Material>::ASSET_TYPE) {
                                material = &std::static_pointer_cast<Asset<Material>>(loaded)->Data();
                            } else if (loaded->GetType() == AssetTraits<MaterialInstance>::ASSET_TYPE) {
                                const auto &instance = std::static_pointer_cast<Asset<MaterialInstance>>(loaded)->Data();
                                if (instance.version != MaterialInstanceAssetData::CURRENT_VERSION || !instance.material)
                                    return Invalid("Scene material instance has an incompatible version or missing parent");
                                const auto parent = Load(instance.material);
                                if (!parent || !(parent->GetType() == AssetTraits<Material>::ASSET_TYPE) ||
                                    parent->GetStatus() == AssetBase::Status::FAILED)
                                    return Invalid("Scene material instance parent is missing or invalid");
                                if (!parent->IsLoaded())
                                    return {};
                                instanceMaterial = std::static_pointer_cast<Asset<Material>>(parent)->Data();
                                std::set<std::string> overrides;
                                for (const auto &value : instance.valueOverrides) {
                                    const auto property = std::ranges::find(instanceMaterial.properties, value.name, &MaterialPropertyData::name);
                                    if (property == instanceMaterial.properties.end() || !overrides.insert(value.name).second ||
                                        property->type != value.type || !MaterialPropertyValueMatches(value.type, value.value) ||
                                        IsTextureProperty(value.type))
                                        return Invalid("Material instance value override has an unknown name or incompatible semantic type");
                                    property->value = value.value;
                                }
                                for (const auto &texture : instance.textureOverrides) {
                                    const auto property = std::ranges::find(instanceMaterial.properties, texture.name, &MaterialPropertyData::name);
                                    if (property == instanceMaterial.properties.end() || !overrides.insert(texture.name).second ||
                                        !IsTextureProperty(property->type))
                                        return Invalid("Material instance texture override has an unknown name or incompatible semantic type");
                                    property->value = texture.texture;
                                }
                                instanceOverrides = std::move(overrides);
                                material          = &instanceMaterial;
                            } else {
                                return Invalid("Scene material asset type mismatch");
                            }
                        }
                        std::vector<RenderResourcePayload> images;
                        bool                               pending = false;
                        auto                               ready   = MaterialPayload(
                            materialId, material, info, skeletal,
                            RenderVertexMask(result->payloads.front().mesh->streams, result->payloads.front().mesh->attributes), images, pending);
                        if (pending)
                            return {};
                        if (const auto *error = std::get_if<RendererError>(&ready)) {
                            return *error;
                        }
                        auto payload = std::move(std::get<RenderResourcePayload>(ready));
                        for (const auto &key : payload.material->schema.keys)
                            if (key.kind == MaterialKeyKind::OPTION && instanceOverrides.contains(key.name))
                                return Invalid("MaterialInstance cannot override a static Shader option: " + key.name);
                        materials.emplace(materialId, payload.resource);
                        surfaces[materialId] =
                            !material ? SceneSurfaceFacts::StaticOpaque(loadedRevision)
                                      : (capabilities && capabilities->resolve ? capabilities->resolve(materialId, *material, loadedRevision)
                                                                               : SceneSurfaceFacts{});
                        if (material) {
                            const auto opaque = std::find_if(material->techniques.begin(), material->techniques.end(),
                                                             [](const auto &technique) { return technique.tag == "opaque"; });
                            if (opaque == material->techniques.end() || !opaque->renderState.depthStencil.depthTest ||
                                !opaque->renderState.depthStencil.depthWrite ||
                                (opaque->renderState.depthStencil.compareOp != CompareOp::LESS &&
                                 opaque->renderState.depthStencil.compareOp != CompareOp::LESS_OR_EQUAL) ||
                                std::any_of(opaque->renderState.blendStates.begin(), opaque->renderState.blendStates.end(),
                                            [](const auto &blend) { return bool(blend.blendEn); }) ||
                                surfaces[materialId].cull != SceneSurfaceCull::NONE)
                                surfaces[materialId] = {};
                        }
                        for (auto &image : images)
                            if (std::none_of(result->payloads.begin(), result->payloads.end(),
                                             [&](const auto &existing) { return existing.resource == image.resource; }))
                                result->payloads.push_back(std::move(image));
                        result->payloads.push_back(std::move(payload));
                    }
                    result->parts.push_back({.part     = part,
                                             .mesh     = {.resource = meshKey, .bindingRevision = 1},
                                             .material = {.resource = materials.at(materialId), .bindingRevision = 1},
                                             .surface  = surfaces.at(materialId)});
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
                Refresh();
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
                std::get<RenderResourcePayload>(converted).resource.revision = loadedRevision;
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
                if (!watches.contains(id) && watches.size() >= 1024)
                    return {};
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
                    if (!watches.contains(id))
                        watches.emplace(id, std::make_unique<AssetWatch>(id, *this));
                }
                return result;
            }
            static std::variant<RendererError, ShaderDataValue> TypedProperty(const MaterialPropertyData &property)
            {
                return std::visit(
                    [&](const auto &value) -> std::variant<RendererError, ShaderDataValue> {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (ShaderDataTypeOf<T>() == ShaderDataType::UNKNOWN) {
                            return RendererError{.code    = RendererErrorCode::UNSUPPORTED,
                                                 .message = "Material property type is not supported for shader data"};
                        } else {
                            auto        data = MakeShaderData(value);
                            std::string error;
                            if (!ValidateShaderData(data, error))
                                return Invalid(std::move(error));
                            if constexpr (!std::is_integral_v<T>) {
                                for (size_t i = 0; i < data.bytes.size(); i += sizeof(float)) {
                                    float component;
                                    std::memcpy(&component, data.bytes.data() + i, sizeof(component));
                                    if (!std::isfinite(component))
                                        return Invalid("Material shader data must be finite");
                                }
                            }
                            return data;
                        }
                    },
                    property.value);
            }
            static std::optional<ShaderTarget> Target(API backend)
            {
                switch (backend) {
                case API::VULKAN: return ShaderTarget::SPIRV;
                case API::DX12: return ShaderTarget::DXIL;
                case API::METAL: return ShaderTarget::MSL;
                default: return {};
                }
            }
            const GlobalVariantLayout &PipelineLayout()
            {
                if (!pipelineLayoutReady) {
                    pipelineLayoutReady = products ? pipelineLayout.LoadPublished(*products, &pipelineError)
                                                   : pipelineLayout.Load(*files, "aurora/config/pipeline_variants.slang", &pipelineError);
                }
                return pipelineLayout;
            }
            std::variant<RendererError, ShaderTechniqueVariant> ResolveTechniqueVariant(const ShaderTechnique                &technique,
                                                                                        API                                   backend,
                                                                                        VertexSemanticMask                    mask,
                                                                                        const ShaderVariant                  &pipelineValues,
                                                                                        std::span<const MaterialPropertyData> properties)
            {
                const auto target = Target(backend);
                if (!target) {
                    return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Technique backend is unsupported"};
                }
                ShaderResolver      resolver(products.Get(), nullptr, *files, 0, 0,
                                        products ? ShaderResolveMode::PRODUCTS : ShaderResolveMode::DEVELOPMENT);
                ShaderVariantSchema schema;
                uint64_t            schemaFp = 0;
                std::string         error;
                if (!resolver.ResolveSchema(technique.vertex.relativePath, static_cast<uint32_t>(*target), schema, schemaFp, &error)) {
                    return Invalid(error);
                }
                schema = SelectVariantDomain(schema, false);
                ShaderVariantSchema fragment;
                if (!resolver.ResolveSchema(technique.fragment.relativePath, static_cast<uint32_t>(*target), fragment, schemaFp, &error))
                    return Invalid(error);
                if (!MergeStageVariantSchemas(schema, SelectVariantDomain(fragment, false), schema, error))
                    return Invalid(error);
                ShaderVariant localValues = technique.variant;
                if (!AppendMaterialShaderOptions(schema, properties, localValues, error))
                    return Invalid(error);
                ShaderVariantInputs inputs;
                inputs.global         = &PipelineLayout();
                inputs.schema         = &schema;
                inputs.vertexMask     = mask;
                inputs.pipelineValues = &pipelineValues;
                inputs.shaderValues   = &localValues;
                ShaderVariantInfo info;
                if (!BuildShaderVariant(inputs, info, &error)) {
                    return Invalid(error);
                }
                ShaderTechniqueVariant result;
                result.macros         = std::move(info.values);
                result.vertexFeatures = inputs.vertexMask;
                result.variantHash    = info.variantHash;
                result.key            = info.key;
                result.dump           = result.macros.ToString();
                result.schema         = MergeVariantSchemas(schema, &PipelineLayout().schema);
                return result;
            }
            static uint64_t LayoutFingerprint(const ShaderReflection &reflection, uint64_t seed)
            {
                auto mix = [](uint64_t hash, uint64_t value) {
                    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
                    return hash;
                };
                for (const auto &input : reflection.vertexInputs) {
                    uint64_t value = static_cast<uint64_t>(input.location) << 32 | static_cast<uint64_t>(input.components) << 8 |
                                     static_cast<uint64_t>(input.scalarType);
                    for (const unsigned char c : input.semantic) {
                        value = mix(value, c);
                    }
                    seed = mix(seed, value);
                }
                for (const auto &resource : reflection.resources) {
                    seed = mix(seed, static_cast<uint64_t>(resource.type) << 48 | static_cast<uint64_t>(resource.set) << 32 |
                                         static_cast<uint64_t>(resource.binding));
                }
                return seed;
            }
            std::optional<RendererError> BindMaterialParameters(std::span<const MaterialPropertyData> properties,
                                                                RenderMaterialTechnique              &technique,
                                                                RendererInfo                          info,
                                                                std::vector<RenderResourcePayload>   &images,
                                                                bool                                 &pending)
            {
                auto &program = technique.program;
                program.parameterAliases.clear();
                technique.parameters.set = shared::MaterialBlockBindings::gMaterial.set;
                ShaderParameterTable table;
                std::string          error;
                if (!BuildShaderParameterTable(program.reflection, 1, program.parameterAliases, table, error))
                    return Invalid(std::move(error));
                for (const auto &propertyValue : properties) {
                    const auto           *property = &propertyValue;
                    const Name            name(property->name.c_str());
                    ShaderParameterHandle handle;
                    const auto            status = table.Resolve(name, shared::MaterialBlockBindings::gMaterial.set, handle);
                    if (status == ShaderParameterLookup::UNKNOWN)
                        continue;
                    if (status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE)
                        return Invalid("Material key is ambiguous or stale: " + property->name);
                    const ShaderParameterEntry *entry = nullptr;
                    table.Resolve(handle, entry);
                    if (!entry)
                        return Invalid("Material parameter table entry is unavailable");
                    const auto declaration = std::ranges::find_if(program.reflection.declarations, [&](const auto &resource) {
                        return resource.set == entry->set && resource.binding == entry->binding;
                    });
                    if (declaration == program.reflection.declarations.end())
                        return Invalid("Material parameter has no logical declaration");
                    if (property->type == MaterialPropertyType::SAMPLER) {
                        const auto *sampler = std::get_if<Sampler::Descriptor>(&property->value);
                        if (!sampler || entry->kind != ShaderParameterKind::RESOURCE || entry->resourceType != ShaderResourceType::SAMPLER ||
                            declaration->count != 1 || declaration->descriptorArray || declaration->sampler != ShaderSamplerType::REGULAR)
                            return Invalid("Material sampler alias has an incompatible target");
                        technique.parameters.samplers.push_back({name, *sampler});
                    } else if (IsTextureProperty(property->type)) {
                        if (property->type != MaterialPropertyType::TEXTURE2D || entry->kind != ShaderParameterKind::RESOURCE ||
                            entry->resourceType != ShaderResourceType::SAMPLED_IMAGE || declaration->count != 1 || declaration->descriptorArray ||
                            declaration->texture.dimension != ShaderTextureDimension::D2 || declaration->texture.arrayed ||
                            declaration->texture.multisampled || declaration->texture.scalarType != ShaderScalarType::FLOAT ||
                            declaration->texture.components != 4)
                            return RendererError{.code    = RendererErrorCode::UNSUPPORTED,
                                                 .message = "Material images currently require ordinary 2D sampled textures"};
                        const bool  required = std::ranges::any_of(technique.variants, [&](const auto &variant) {
                            return std::ranges::any_of(variant.program.reflection.resources, [&](const auto &resource) {
                                return resource.set == entry->set && resource.binding == entry->binding;
                            });
                        });
                        const auto *id       = std::get_if<Uuid>(&property->value);
                        if (!id)
                            return Invalid("Material texture value has the wrong type");
                        if (!required)
                            continue;
                        if (!*id)
                            return Invalid("Active material texture asset is missing");
                        const auto image = Load(*id);
                        if (!image || !(image->GetType() == AssetTraits<Texture>::ASSET_TYPE) || image->GetStatus() == AssetBase::Status::FAILED)
                            return Invalid("Material texture asset is missing, failed, or has the wrong type");
                        if (!image->IsLoaded()) {
                            pending = true;
                            return {};
                        }
                        if (image->GetUuid() != *id)
                            return Invalid("Material image loader returned a different asset identity");
                        auto converted = ConvertSceneImage(*std::static_pointer_cast<Asset<Texture>>(image), info, SceneImageUsage::MATERIAL);
                        if (const auto *failure = std::get_if<RendererError>(&converted))
                            return *failure;
                        auto payload              = std::move(std::get<RenderResourcePayload>(converted));
                        payload.resource.revision = loadedRevision;
                        technique.parameters.images.push_back({name, payload.resource});
                        if (std::none_of(images.begin(), images.end(), [&](const auto &existing) { return existing.resource == payload.resource; }))
                            images.push_back(std::move(payload));
                    } else {
                        auto value = TypedProperty(*property);
                        if (const auto *failure = std::get_if<RendererError>(&value))
                            return *failure;
                        auto &data = std::get<ShaderDataValue>(value);
                        if (entry->kind != ShaderParameterKind::FIELD || entry->dataType != data.type || entry->arrayCount != data.count)
                            return Invalid("Material data alias has an incompatible semantic type or array count");
                        technique.parameters.values.push_back({name, std::move(data)});
                    }
                }
                if (!ValidateRenderMaterialParameters(program, technique.parameters, info.deviceEpoch, error))
                    return Invalid(std::move(error));
                return {};
            }
            std::variant<RendererError, RenderResourcePayload> MaterialPayload(const Uuid                         &id,
                                                                               const MaterialAssetData            *data,
                                                                               RendererInfo                        info,
                                                                               bool                                skeletal,
                                                                               VertexSemanticMask                  vertexMask,
                                                                               std::vector<RenderResourcePayload> &images,
                                                                               bool                               &pending)
            {
                MaterialAssetData defaults;
                if (data == nullptr) {
                    const std::string     path = id == SceneLitMaterialId() ? "aurora/materials/Lit.slang" : "aurora/materials/Unlit.slang";
                    MaterialTechniqueData opaque;
                    opaque.tag     = "opaque";
                    opaque.program = {.vertex = {.relativePath = path, .entry = "VSMain"}, .fragment = {.relativePath = path, .entry = "FSMain"}};
                    opaque.aliases = {{"tint", "gMaterial.tint"}, {"surface", "gMaterial.surface"}};
                    opaque.renderState.depthStencil.depthTest = opaque.renderState.depthStencil.depthWrite = true;
                    opaque.renderState.depthStencil.compareOp                                              = CompareOp::LESS_OR_EQUAL;
                    auto shadow                                                                            = opaque;
                    shadow.tag                                                                             = "shadow";
                    shadow.program = {.vertex   = {.relativePath = "aurora/materials/Shadow.slang", .entry = "VSMain"},
                                      .fragment = {.relativePath = "aurora/materials/Shadow.slang", .entry = "FSMain"}};
                    shadow.aliases.clear();
                    defaults.techniques = {opaque, shadow};
                    defaults.properties = {{.name = "tint", .type = MaterialPropertyType::FLOAT4, .value = Vector4{.65f, .72f, .82f, 1}},
                                           {.name = "surface", .type = MaterialPropertyType::FLOAT4, .value = Vector4{.5f, 0, 0, 0}}};
                    data                = &defaults;
                }
                std::string validation;
                if (!ValidateMaterialAssetData(*data, validation))
                    return Invalid(std::move(validation));
                std::set<std::string> properties;
                std::set<std::string> tags;
                for (const auto &property : data->properties) {
                    if (property.name.empty() || !properties.insert(property.name).second) {
                        return Invalid("Material properties require unique names");
                    }
                }
                RenderResourcePayload result;
                result.format     = Name("runtime-material");
                auto    &material = result.material.emplace();
                uint64_t layoutFp = HashMixU64(0xcbf29ce484222325ull, vertexMask.mask);
                PipelineLayout();
                if (!pipelineLayoutReady)
                    return Invalid(pipelineError);
                const auto pipelineVariants = ScenePipelineRequests(PipelineLayout().schema);
                if (pipelineVariants.empty()) {
                    return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Pipeline variant space exceeds the supported budget"};
                }
                for (const auto &authored : data->techniques) {
                    if (authored.tag.empty() || !tags.insert(authored.tag).second || !authored.program.compute.relativePath.empty()) {
                        return Invalid("Scene material requires unique Tags and graphics techniques");
                    }
                    RenderMaterialTechnique technique;
                    technique.tag = Name(authored.tag.c_str());
                    for (const auto &pipelineValues : pipelineVariants) {
                        const auto variant = ResolveTechniqueVariant(authored.program, info.backend, vertexMask, pipelineValues, data->properties);
                        if (const auto *error = std::get_if<RendererError>(&variant)) {
                            return *error;
                        }
                        const auto &resolved = std::get<ShaderTechniqueVariant>(variant);
                        auto        compiled = CompileShaderTechnique(*files, info.backend, authored.program, products.Get(), &resolved);
                        if (const auto *error = std::get_if<RendererError>(&compiled)) {
                            return *error;
                        }
                        auto        program    = std::move(std::get<RenderTechniqueData>(compiled));
                        const auto &palette    = shared::GetSkinningBatchReflection().resources.front();
                        const auto  slot       = std::ranges::find_if(program.reflection.resources, [&](const auto &resource) {
                            return resource.set == palette.set && resource.binding == palette.binding;
                        });
                        const bool  hasPalette = slot != program.reflection.resources.end();
                        if ((skeletal && (!hasPalette || slot->type != palette.type || slot->bufferKind != palette.bufferKind ||
                                          slot->elementStride != palette.elementStride || slot->access != ShaderResourceAccess::READ_ONLY ||
                                          slot->count != 1 || slot->descriptorArray)) ||
                            (!skeletal && hasPalette)) {
                            return RendererError{RendererErrorCode::UNSUPPORTED,
                                                 "Material technique does not match the component SkinningBatch palette contract: " + authored.tag};
                        }
                        const auto &state  = authored.renderState;
                        const auto &raster = state.rasterState;
                        if (state.alphaToCoverage || state.depthStencil.stencilTest || state.depthStencil.minDepth != 0 ||
                            state.depthStencil.maxDepth != 1 || raster.depthClampEnable || raster.rasterizerDiscardEnable || raster.depthBiasEnable ||
                            raster.depthClipEnable || raster.lineWidth != 1 || raster.cullMode != CullModeFlagBits::NONE ||
                            raster.polygonMode != PolygonMode::FILL) {
                            return RendererError{.code    = RendererErrorCode::UNSUPPORTED,
                                                 .message = "Scene material raster/stencil state is unsupported"};
                        }
                        program.depth = {.test    = bool(state.depthStencil.depthTest),
                                         .write   = bool(state.depthStencil.depthWrite),
                                         .compare = state.depthStencil.compareOp};
                        if (state.blendStates.size() > 1) {
                            return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Scene material has multiple blend targets"};
                        }
                        if (!state.blendStates.empty()) {
                            program.blend = state.blendStates.front();
                        }
                        ShaderVariant pipeline;
                        for (const auto &entry : resolved.macros.entries)
                            if (const auto *definition = PipelineLayout().schema.FindEntry(entry.key);
                                definition && entry.value != definition->defaultValue)
                                pipeline.entries.push_back(entry);
                        technique.variants.push_back(
                            {.program = std::move(program), .pipeline = std::move(pipeline), .skinned = skeletal, .vertexMask = vertexMask.mask});
                    }
                    technique.program = technique.variants.front().program;
                    MaterialSchema generated;
                    if (!BuildMaterialSchema(technique.program.reflection, technique.program.variantSchema, generated, validation) ||
                        !MergeMaterialSchemas(generated, material.schema, validation))
                        return Invalid(validation);
                    for (auto &variant : technique.variants) {
                        if (RenderMaterialInterfaceFingerprint(variant.program, 2) != RenderMaterialInterfaceFingerprint(technique.program, 2))
                            return Invalid("Material declaration changes across Pipeline or Vertex variants");
                    }
                    layoutFp = LayoutFingerprint(technique.program.reflection, layoutFp);
                    material.techniques.push_back(std::move(technique));
                }
                std::map<std::string, std::string> migratedNames;
                for (size_t i = 0; i < data->techniques.size(); ++i) {
                    const auto          &authored = data->techniques[i];
                    ShaderParameterTable table;
                    if (!BuildShaderParameterTable(material.techniques[i].program.reflection, 1, {}, table, validation))
                        return Invalid(validation);
                    for (const auto &alias : authored.aliases) {
                        ShaderParameterHandle handle;
                        const auto            status = table.Resolve(Name(alias.target.c_str()), 2, handle);
                        if (!properties.contains(alias.alias) ||
                            (status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE))
                            return Invalid("Legacy material alias requires an existing value and exact Group 2 key: " + alias.alias);
                        const auto [found, inserted] = migratedNames.emplace(alias.alias, alias.target);
                        if (!inserted && found->second != alias.target)
                            return Invalid("Material legacy aliases disagree: " + alias.alias);
                    }
                }
                std::vector<MaterialPropertyData> canonical;
                std::set<std::string>             canonicalNames;
                for (const auto &property : data->properties) {
                    auto value = property;
                    if (const auto found = migratedNames.find(value.name); found != migratedNames.end())
                        value.name = found->second;
                    const auto key = std::ranges::find(material.schema.keys, value.name, &MaterialKey::name);
                    if (key == material.schema.keys.end() || !canonicalNames.insert(value.name).second)
                        return Invalid("Material value has an unknown or conflicting canonical key: " + value.name);
                    if (key->kind != MaterialKeyKind::OPTION)
                        canonical.push_back(std::move(value));
                }
                for (size_t i = 0; i < material.techniques.size(); ++i) {
                    auto &technique = material.techniques[i];
                    if (const auto error = BindMaterialParameters(canonical, technique, info, images, pending))
                        return *error;
                    if (pending)
                        return RenderResourcePayload{};
                    for (const auto &variant : technique.variants)
                        if (!ValidateRenderMaterialParameters(variant.program, technique.parameters, info.deviceEpoch, validation))
                            return Invalid(validation);
                }
                // usageVariant is the layout-compatible family, not a program alias.
                result.resource = {
                    .product = id, .revision = loadedRevision, .usageVariant = layoutFp == 0 ? 1ull : layoutFp, .deviceEpoch = info.deviceEpoch};
                return result;
            }
            std::shared_ptr<ShaderFileSystem>                                                                          files;
            std::function<AssetPtr(const Uuid &)>                                                                      loader;
            std::map<Uuid, AssetPtr>                                                                                   assets;
            std::map<std::tuple<Uuid, Uuid, API, uint64_t, bool>, std::shared_ptr<const SceneMeshResources>>           cache;
            std::map<std::tuple<Uuid, Uuid, Uuid, Uuid, API, uint64_t>, std::shared_ptr<const SceneSkeletalResources>> skeletalCache;
            SkinBindingCache                                                                                           skinBindings;
            std::map<std::pair<Uuid, uint64_t>, std::shared_ptr<const RenderResourcePayload>>                          imageCache;
            std::atomic<uint64_t>                                                                                      revision{1};
            uint64_t                                                                                                   loadedRevision = 1;
            std::map<Uuid, std::optional<AABB>>                                                                        bounds;
            std::map<Uuid, std::unique_ptr<AssetWatch>>                                                                watches;
            uint64_t                                                                                                   cachedBytes = 0;
            FileSystemPtr                                                                                              products;
            std::shared_ptr<const SceneMaterialCapabilitySource>                                                       capabilities;
            GlobalVariantLayout                                                                                        pipelineLayout;
            bool                                                                                                       pipelineLayoutReady = false;
            std::string                                                                                                pipelineError;
        };
    } // namespace
    std::shared_ptr<ISceneResourceSource> CreateSceneAssetSource(std::shared_ptr<ShaderFileSystem>                    files,
                                                                 std::function<AssetPtr(const Uuid &)>                loader,
                                                                 FileSystemPtr                                        products,
                                                                 std::shared_ptr<const SceneMaterialCapabilitySource> capabilities)
    {
        if (!files) {
            files = std::make_shared<ShaderFileSystem>();
        }
        if (!loader) {
            loader = [](const Uuid &id) { return AssetManager::Get()->LoadAsset(id); };
        }
        return std::make_shared<SceneAssetSource>(std::move(files), std::move(loader), std::move(products), std::move(capabilities));
    }
} // namespace sky::aurora
