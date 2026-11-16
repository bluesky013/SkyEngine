//
// Created on 2026/10/30.
//

#include <recast/render/NavigationRenderBridge.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace sky::ai {
    using namespace aurora;

    namespace {
        uint64_t GeometryHash(const NaviDebugGeometry &geometry)
        {
            uint64_t hash = 1469598103934665603ull;
            for (const auto &vertex : geometry.vertices) {
                for (const float value : {vertex.position.x, vertex.position.y, vertex.position.z}) {
                    hash = (hash ^ std::bit_cast<uint32_t>(value)) * 1099511628211ull;
                }
            }
            return hash;
        }

        void ConfigureProgram(RenderTechniqueData &program)
        {
            program.depth          = {true, false, CompareOp::LESS_OR_EQUAL};
            program.blend.blendEn  = true;
            program.blend.srcColor = BlendFactor::SRC_ALPHA;
            program.blend.dstColor = BlendFactor::ONE_MINUS_SRC_ALPHA;
            program.blend.srcAlpha = BlendFactor::ONE;
            program.blend.dstAlpha = BlendFactor::ONE_MINUS_SRC_ALPHA;
        }
    } // namespace

    bool NavigationRenderBridge::PrepareMaterial(FramePacket &packet, RendererInfo info, ISceneResourceSource &resources, const Uuid &asset)
    {
        const auto sourceRevision = resources.Revision();
        if (material.product && materialAsset == asset && resourceRevision == sourceRevision)
            return true;
        if (!asset) {
            error = "Navigation overlay material asset is unavailable";
            return false;
        }
        VertexSemanticMask vertexMask;
        vertexMask.Set(VertexSemantic::POSITION);
        const auto resolved = resources.ResolveMaterial(asset, info, vertexMask);
        if (const auto *failure = std::get_if<RendererError>(&resolved)) {
            error = failure->message;
            return false;
        }
        const auto *ready = std::get_if<std::shared_ptr<const SceneMaterialResources>>(&resolved);
        if (!ready || !*ready)
            return false;
        if (!(*ready)->material.material) {
            error = "Navigation overlay requires a scene material";
            return false;
        }
        auto copy     = (*ready)->material;
        copy.resource = {Uuid::Create(), 1, 1, info.deviceEpoch};
        for (auto &technique : copy.material->techniques) {
            ConfigureProgram(technique.program);
            for (auto &variant : technique.variants)
                ConfigureProgram(variant.program);
            for (auto &value : technique.parameters.values)
                if (value.name == Name("gMaterial.tint") || value.name == Name("tint"))
                    value.value = MakeShaderData(Vector4{.1f, .85f, .25f, .6f});
        }
        if (std::ranges::any_of((*ready)->dependencies, [](const auto &dependency) { return !dependency; }))
            return false;
        ReleaseMaterial(packet);
        for (const auto &dependency : (*ready)->dependencies) {
            materialDependencies.push_back(dependency->resource);
            packet.commands.emplace_back(ResourceCpuReady{*dependency});
        }
        material         = copy.resource;
        materialAsset    = asset;
        resourceRevision = sourceRevision;
        packet.commands.emplace_back(ResourceCpuReady{std::move(copy)});
        return true;
    }

    bool NavigationRenderBridge::Append(FramePacket             &packet,
                                        RenderEntityRef          slot,
                                        RendererInfo             info,
                                        const NaviDebugGeometry &geometry,
                                        ISceneResourceSource    &resources,
                                        const Uuid              &materialAsset)
    {
        error.clear();
        if (info.deviceEpoch != deviceEpoch) {
            OnRendererStopped();
            deviceEpoch = info.deviceEpoch;
        }
        if (!info.deviceEpoch || !slot.scene.IsValid() || !slot.entity.IsValid()) {
            error = "Navigation overlay requires a live renderer and entity slot";
            return false;
        }
        if (geometry.vertices.empty()) {
            Clear(packet);
            return true;
        }
        if (geometry.vertices.size() % 3 || geometry.vertices.size() > 3 * 1024 * 1024 ||
            !std::ranges::all_of(geometry.vertices, [](const auto &vertex) {
                return std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z);
            })) {
            error = "Navigation debug geometry must contain finite, complete triangles within the vertex budget";
            return false;
        }
        if (created && (entity.scene != slot.scene || entity.entity.index != slot.entity.index))
            Clear(packet);
        if (retiring)
            return true;
        const auto previousMaterial = material;
        if (!PrepareMaterial(packet, info, resources, materialAsset))
            return false;
        const auto hash = GeometryHash(geometry);
        if (created && hash == geometryHash && previousMaterial == material)
            return true;
        if (revision == std::numeric_limits<uint64_t>::max() - 1) {
            error = "Navigation overlay revision exhausted";
            return false;
        }
        const auto            count = static_cast<uint32_t>(geometry.vertices.size());
        RenderResourcePayload payload;
        payload.resource       = {mesh.product ? mesh.product : Uuid::Create(), revision + 1, 1, info.deviceEpoch};
        payload.format         = Name("runtime-mesh");
        const auto vertexBytes = geometry.vertices.size() * sizeof(Vector3);
        payload.bytes.resize(vertexBytes + geometry.vertices.size() * sizeof(uint32_t));
        AABB bounds{geometry.vertices.front().position, geometry.vertices.front().position};
        for (uint32_t i = 0; i < count; ++i) {
            auto position = geometry.vertices[i].position;
            position.y += .02f;
            std::memcpy(payload.bytes.data() + i * sizeof(Vector3), &position, sizeof(position));
            std::memcpy(payload.bytes.data() + vertexBytes + i * sizeof(uint32_t), &i, sizeof(i));
            bounds.min = {std::min(bounds.min.x, position.x), std::min(bounds.min.y, position.y), std::min(bounds.min.z, position.z)};
            bounds.max = {std::max(bounds.max.x, position.x), std::max(bounds.max.y, position.y), std::max(bounds.max.z, position.z)};
        }
        payload.metadata.bounds = bounds;
        payload.mesh            = RenderMeshData{{{0, vertexBytes, sizeof(Vector3)}},
                                                 {{0, 0, 0, Format::F_RGB32, "POSITION", 0}},
                                                 {{vertexBytes, size_t(count) * sizeof(uint32_t), IndexType::U32, {count, 1}}}};
        RenderEntityDesc desc;
        desc.mesh     = {payload.resource, revision + 1};
        desc.material = {material, revision + 1};
        // The scene pass sorts tags first; draw translucent debug surfaces after ordinary geometry.
        desc.properties.tags       = (uint64_t{1} << 63) | RenderDrawProperties{}.tags;
        desc.properties.castShadow = desc.properties.receiveShadow = false;
        entity                                                     = {slot.scene, {slot.entity.index, generation}};
        if (created)
            packet.commands.emplace_back(UpdateRenderEntity{entity, desc, revision + 1});
        else
            packet.commands.emplace_back(CreateRenderEntity{entity, desc});
        if (mesh.product)
            packet.releasedResources.push_back(mesh);
        mesh = payload.resource;
        packet.commands.emplace_back(ResourceCpuReady{std::move(payload)});
        geometryHash = hash;
        ++revision;
        created = true;
        return true;
    }

    void NavigationRenderBridge::Clear(FramePacket &packet)
    {
        if (created && !retiring) {
            packet.commands.emplace_back(DestroyRenderEntity{entity});
            retiring = true;
        }
        if (mesh.product)
            packet.releasedResources.push_back(mesh);
        ReleaseMaterial(packet);
        mesh         = {};
        geometryHash = 0;
    }

    void NavigationRenderBridge::ReleaseMaterial(FramePacket &packet)
    {
        if (material.product)
            packet.releasedResources.push_back(material);
        packet.releasedResources.insert(packet.releasedResources.end(), materialDependencies.begin(), materialDependencies.end());
        materialDependencies.clear();
        material         = {};
        materialAsset    = {};
        resourceRevision = 0;
    }

    void NavigationRenderBridge::OnEvent(const RendererEvent &event)
    {
        if (const auto *destroyed = std::get_if<EntityDestroyed>(&event); destroyed && destroyed->entity == entity) {
            created = retiring = false;
            if (generation != std::numeric_limits<uint32_t>::max())
                ++generation;
        }
        if (const auto *destroyed = std::get_if<SceneDestroyed>(&event); destroyed && destroyed->scene == entity.scene) {
            created = retiring = false;
            entity             = {};
            generation         = 1;
        }
        if (std::holds_alternative<RendererStopped>(event))
            OnRendererStopped();
    }

    void NavigationRenderBridge::OnRendererStopped()
    {
        created = retiring = false;
        entity             = {};
        mesh = material = {};
        materialDependencies.clear();
        materialAsset    = {};
        resourceRevision = 0;
        deviceEpoch = geometryHash = revision = 0;
        generation                            = 1;
        error.clear();
    }

} // namespace sky::ai
