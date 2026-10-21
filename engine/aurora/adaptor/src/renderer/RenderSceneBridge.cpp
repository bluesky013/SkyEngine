#include <aurora/adaptor/renderer/RenderSceneBridge.h>

#include <algorithm>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/SpotLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <cmath>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <map>
#include <numbers>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        using Key = std::tuple<Uuid, Uuid, uint32_t>;
        auto ResourceKey(const RenderResourceKey &key)
        {
            return std::tuple{key.product, key.revision, key.usageVariant, key.deviceEpoch};
        }
        bool Same(const RenderEntityDesc &a, const RenderEntityDesc &b)
        {
            const auto &x = a.properties.light;
            const auto &y = b.properties.light;
            return std::equal(std::begin(a.world.v), std::end(a.world.v), std::begin(b.world.v)) && a.mesh.resource == b.mesh.resource &&
                   a.material.resource == b.material.resource && a.visibilityMask == b.visibilityMask && a.properties.tags == b.properties.tags &&
                   a.properties.subMesh == b.properties.subMesh && a.properties.castShadow == b.properties.castShadow &&
                   a.properties.receiveShadow == b.properties.receiveShadow && x.color == y.color && x.intensity == y.intensity &&
                   x.range == y.range && x.kind == y.kind && x.direction == y.direction && x.innerConeAngle == y.innerConeAngle &&
                   x.outerConeAngle == y.outerConeAngle && x.castShadow == y.castShadow;
        }
        Matrix4 WorldMatrix(Actor &actor)
        {
            const auto *transform = actor.GetComponent<TransformComponent>();
            return (transform != nullptr) ? transform->GetWorldMatrix() : RenderIdentityMatrix();
        }
        Vector3 Direction(const Matrix4 &world)
        {
            const Vector3 v{-world.v[8], -world.v[9], -world.v[10]};
            const float   length = std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
            return length > 0.00001f ? v / length : Vector3{0, 0, -1};
        }
    } // namespace

    struct RenderSceneBridge::Impl final : IWorldEvent, ISceneComponentEvent {
        struct Slot {
            RenderEntityRef  id;
            RenderEntityDesc desc;
            uint64_t         revision = 0;
            bool             occupied = false, retiring = false, destroyQueued = false;
        };
        struct TransformWatch final : ITransformEvent {
            explicit TransformWatch(Actor *actor) : actor(actor)
            {
                TransformEvent::Connect(actor, this);
            }
            ~TransformWatch()
            {
                TransformEvent::DisConnect(actor, this);
            }
            void OnTransformChanged(const Transform &, const Transform &) override
            {
                dirty = true;
            }
            const Matrix4 &Read()
            {
                if (dirty) {
                    matrix = WorldMatrix(*actor);
                    dirty  = false;
                }
                return matrix;
            }
            Actor  *actor;
            bool    dirty = true;
            Matrix4 matrix;
        };
        Impl(RendererInfo value, std::shared_ptr<ISceneResourceSource> source) : info(value), resources(std::move(source))
        {
        }
        ~Impl()
        {
            Bind(nullptr);
        }

        void Remove(const Uuid &actor, const Uuid *component = nullptr)
        {
            std::erase_if(active, [&](const auto &item) {
                if (std::get<0>(item.first) != actor || (component && std::get<1>(item.first) != *component)) {
                    return false;
                }
                slots[item.second].retiring = true;
                return true;
            });
        }
        void OnActorAttached(Actor *actor) override
        {
            if (!watches.contains(actor->GetUuid())) {
                watches.emplace(actor->GetUuid(), std::make_unique<TransformWatch>(actor));
            }
        }
        void OnActorDetached(Actor *actor) override
        {
            Remove(actor->GetUuid());
            watches.erase(actor->GetUuid());
        }
        void OnSceneComponentChanged(ComponentBase &component, bool attached) override
        {
            if (!attached && (component.GetActor() != nullptr)) {
                Remove(component.GetActor()->GetUuid(), &component.GetTypeId());
            }
        }
        void Bind(World *next)
        {
            if (world.Get() == next) {
                return;
            }
            if (world) {
                WorldEvent::DisConnect(world.Get(), this);
                Event<ISceneComponentEvent>::DisConnect(world.Get(), this);
            }
            watches.clear();
            for (const auto &[key, index] : active) {
                slots[index].retiring = true;
            }
            active.clear();
            world = next;
            if (world) {
                WorldEvent::Connect(world.Get(), this);
                Event<ISceneComponentEvent>::Connect(world.Get(), this);
                for (const auto &actor : world->GetActors()) {
                    OnActorAttached(actor.get());
                }
            }
        }
        std::optional<RendererError> Upsert(FramePacket &packet, SceneId scene, const Key &key, RenderEntityDesc desc)
        {
            if (!IsValidRenderEntityDesc(desc)) {
                return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Scene component contains invalid render values"};
            }
            const auto found = active.find(key);
            if (found != active.end()) {
                auto &slot = slots[found->second];
                if (!Same(slot.desc, desc)) {
                    if (slot.revision >= UINT64_MAX - 1) {
                        return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene binding revision exhausted"};
                    }
                    ++slot.revision;
                    desc.mesh.bindingRevision = desc.material.bindingRevision = slot.revision + 1;
                    slot.desc                                                 = desc;
                    packet.commands.emplace_back(UpdateRenderEntity{.id = slot.id, .desc = desc, .revision = slot.revision});
                }
                return {};
            }
            size_t index = 0;
            while (index < slots.size() && (slots[index].occupied || slots[index].id.entity.generation == UINT32_MAX)) {
                ++index;
            }
            if (index == slots.size()) {
                if (index >= 8192) {
                    return RendererError{.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene bridge entity budget exceeded"};
                }
                slots.emplace_back();
            }
            auto &slot    = slots[index];
            slot.id       = {.scene = scene, .entity = {static_cast<uint32_t>(index), slot.id.entity.generation + 1}};
            slot.occupied = true;
            slot.retiring = slot.destroyQueued = false;
            slot.revision                      = 0;
            desc.mesh.bindingRevision = desc.material.bindingRevision = 1;
            slot.desc                                                 = desc;
            active.emplace(key, static_cast<uint32_t>(index));
            packet.commands.emplace_back(CreateRenderEntity{.id = slot.id, .desc = desc});
            return {};
        }
        std::optional<RendererError> Append(FramePacket &packet, SceneId scene, ViewportId output)
        {
            issues.clear();
            if (!scene.IsValid() || (packet.sequence == 0u) || (info.deviceEpoch == 0u) || !resources) {
                return RendererError{.code    = RendererErrorCode::INVALID_ARGUMENT,
                                     .message = "Scene bridge requires a scene, packet sequence and resource source"};
            }
            if (boundScene.IsValid() && boundScene != scene) {
                return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Scene bridge cannot change renderer scene while alive"};
            }
            boundScene = scene;
            std::set<Key>                                          seen;
            std::map<decltype(ResourceKey({})), RenderResourceKey> used;
            const auto                                             add = [&](const Key &key, RenderEntityDesc desc) {
                seen.insert(key);
                if (const auto error = Upsert(packet, scene, key, desc)) {
                    issues.push_back(*error);
                }
            };
            if (world) {
                for (const auto &actor : world->GetActors()) {
                    const auto watch  = watches.find(actor->GetUuid());
                    const auto matrix = watch != watches.end() ? watch->second->Read() : WorldMatrix(*actor);
                    for (const auto &[type, component] : actor->GetComponents()) {
                        if (type == TypeInfo<StaticMeshComponent>::RegisteredId()) {
                            const auto &data = static_cast<const StaticMeshComponent &>(*component).GetData();
                            if (!data.mesh) {
                                continue;
                            }
                            const auto result = resources->ResolveMesh(data.mesh, data.material, info);
                            if (const auto *error = std::get_if<RendererError>(&result)) {
                                issues.push_back(*error);
                                continue;
                            }
                            const auto *ready = std::get_if<std::shared_ptr<const SceneMeshResources>>(&result);
                            if ((ready == nullptr) || !*ready) {
                                continue;
                            }
                            if ((*ready)->parts.size() > 8192 || (*ready)->payloads.size() > 8193) {
                                issues.push_back({.code = RendererErrorCode::OUT_OF_MEMORY, .message = "Scene mesh part budget exceeded"});
                                continue;
                            }
                            const auto validBinding = [&](const auto &binding) {
                                return binding.resource.product && binding.resource.revision && binding.resource.deviceEpoch == info.deviceEpoch &&
                                       std::any_of((*ready)->payloads.begin(), (*ready)->payloads.end(),
                                                   [&](const auto &payload) { return payload.resource == binding.resource; });
                            };
                            if (std::any_of((*ready)->parts.begin(), (*ready)->parts.end(),
                                            [&](const auto &part) { return !validBinding(part.mesh) || !validBinding(part.material); })) {
                                issues.push_back({.code    = RendererErrorCode::INVALID_ARGUMENT,
                                                  .message = "Scene source returned missing or stale resource versions"});
                                continue;
                            }
                            for (const auto &payload : (*ready)->payloads) {
                                used.emplace(ResourceKey(payload.resource), payload.resource);
                                if (published.emplace(ResourceKey(payload.resource), payload.resource).second) {
                                    packet.commands.emplace_back(ResourceCpuReady{payload});
                                }
                            }
                            for (const auto &part : (*ready)->parts) {
                                RenderEntityDesc desc;
                                desc.world                    = matrix;
                                desc.mesh                     = part.mesh;
                                desc.material                 = part.material;
                                desc.properties.subMesh       = part.part;
                                desc.properties.castShadow    = data.castShadow;
                                desc.properties.receiveShadow = data.receiveShadow;
                                add({actor->GetUuid(), type, part.part}, desc);
                            }
                        } else {
                            RenderEntityDesc desc;
                            desc.world           = matrix;
                            desc.properties.tags = 0;
                            auto &light          = desc.properties.light;
                            if (type == TypeInfo<PointLightComponent>::RegisteredId()) {
                                const auto &data = static_cast<const PointLightComponent &>(*component).GetData();
                                light.color      = data.color;
                                light.intensity  = data.intensity;
                                light.range      = data.range;
                            } else if (type == TypeInfo<DirectLightComponent>::RegisteredId()) {
                                const auto &data = static_cast<const DirectLightComponent &>(*component).GetData();
                                light.color      = data.color;
                                light.intensity  = data.intensity;
                                light.kind       = RenderLightKind::DIRECTIONAL;
                                light.castShadow = data.castShadow;
                            } else if (type == TypeInfo<SpotLightComponent>::RegisteredId()) {
                                const auto &data     = static_cast<const SpotLightComponent &>(*component).GetData();
                                light.color          = data.color;
                                light.intensity      = data.intensity;
                                light.range          = data.range;
                                light.kind           = RenderLightKind::SPOT;
                                light.innerConeAngle = data.innerConeAngle;
                                light.outerConeAngle = data.outerConeAngle;
                            } else {
                                continue;
                            }
                            light.direction = Direction(matrix);
                            add({actor->GetUuid(), type, 0}, desc);
                        }
                    }
                }
            }
            if (world && output.IsValid()) {
                const EnvironmentComponent *environment = nullptr;
                uint32_t                    count       = 0;
                for (const auto &actor : world->GetActors()) {
                    if (const auto *component = actor->GetComponent<EnvironmentComponent>()) {
                        environment = component;
                        ++count;
                    }
                }
                if (count > 1) {
                    issues.push_back({.code = RendererErrorCode::INVALID_ARGUMENT, .message = "World requires at most one environment component"});
                } else if (environment != nullptr) {
                    const auto &data = environment->GetData();
                    if (!std::isfinite(data.exposure) || data.exposure < -16 || data.exposure > 16 || !std::isfinite(data.intensity) ||
                        data.intensity < 0 || data.intensity > 64) {
                        issues.push_back({.code    = RendererErrorCode::INVALID_ARGUMENT,
                                          .message = "Environment exposure or intensity is outside its supported range"});
                    } else {
                        const std::array       ids{data.diffuseIrradiance, data.specularEnvironment, data.brdfIntegration};
                        SceneEnvironmentInputs images;
                        for (size_t i = 0; i < ids.size(); ++i) {
                            if (!data.enabled || !ids[i]) {
                                continue;
                            }
                            const auto result = resources->ResolveImage(ids[i], info);
                            if (const auto *error = std::get_if<RendererError>(&result)) {
                                issues.push_back(*error);
                                continue;
                            }
                            const auto *ready = std::get_if<std::shared_ptr<const RenderResourcePayload>>(&result);
                            if ((ready == nullptr) || !*ready) {
                                continue;
                            }
                            const auto &payload = **ready;
                            if (!payload.resource.product || (payload.resource.revision == 0u) || payload.resource.deviceEpoch != info.deviceEpoch ||
                                !payload.metadata.image || payload.metadata.image->arrayLayers != 1 ||
                                RenderImageByteSize(*payload.metadata.image) != payload.bytes.size()) {
                                issues.push_back(
                                    {.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Environment source returned an invalid image version"});
                                continue;
                            }
                            images[i] = payload.resource;
                            used.emplace(ResourceKey(payload.resource), payload.resource);
                            if (published.emplace(ResourceKey(payload.resource), payload.resource).second) {
                                packet.commands.emplace_back(ResourceCpuReady{payload});
                            }
                        }
                        packet.extensions.push_back(SceneViewInput(
                            Name("aurora.scene"), output,
                            {.hdr = data.hdr ? 1u : 0u, .exposure = data.exposure, .environmentIntensity = data.enabled ? data.intensity : 0.f},
                            images));
                    }
                }
            }
            std::erase_if(active, [&](const auto &item) {
                if (seen.contains(item.first)) {
                    return false;
                }
                slots[item.second].retiring = true;
                return true;
            });
            for (auto &slot : slots) {
                if (slot.occupied && slot.retiring && !slot.destroyQueued) {
                    packet.commands.emplace_back(DestroyRenderEntity{slot.id});
                    slot.destroyQueued = true;
                }
            }
            std::erase_if(published, [&](const auto &item) {
                if (used.contains(item.first)) {
                    return false;
                }
                packet.releasedResources.push_back(item.second);
                return true;
            });
            return {};
        }
        RendererInfo                                           info;
        std::shared_ptr<ISceneResourceSource>                  resources;
        WorldPtr                                               world;
        SceneId                                                boundScene;
        std::map<Uuid, std::unique_ptr<TransformWatch>>        watches;
        std::map<Key, uint32_t>                                active;
        std::vector<Slot>                                      slots;
        std::map<decltype(ResourceKey({})), RenderResourceKey> published;
        std::vector<RendererError>                             issues;
    };

    RenderSceneBridge::RenderSceneBridge(RendererInfo info, std::shared_ptr<ISceneResourceSource> resources)
        : impl(std::make_unique<Impl>(info, std::move(resources)))
    {
    }
    RenderSceneBridge::~RenderSceneBridge() = default;
    void RenderSceneBridge::SetWorld(World *world)
    {
        impl->Bind(world);
    }
    std::optional<RendererError> RenderSceneBridge::Append(FramePacket &packet, SceneId scene, ViewportId output)
    {
        return impl->Append(packet, scene, output);
    }
    const std::vector<RendererError> &RenderSceneBridge::GetIssues() const
    {
        return impl->issues;
    }
    void RenderSceneBridge::OnEvent(const RendererEvent &event)
    {
        if (std::holds_alternative<RendererStopped>(event)) {
            impl->Bind(nullptr);
            impl->slots.clear();
            impl->published.clear();
            impl->boundScene = {};
            return;
        }
        if (const auto *destroyed = std::get_if<EntityDestroyed>(&event)) {
            for (auto &slot : impl->slots) {
                if (slot.occupied && slot.retiring && slot.destroyQueued && slot.id == destroyed->entity) {
                    slot.occupied = false;
                }
            }
        }
        if (const auto *destroyed = std::get_if<SceneDestroyed>(&event); (destroyed != nullptr) && destroyed->scene == impl->boundScene) {
            impl->Bind(nullptr);
            impl->slots.clear();
            impl->published.clear();
            impl->boundScene = {};
        }
        if (const auto *failure = std::get_if<ResourceStateChanged>(&event); (failure != nullptr) && failure->state == RenderResourceState::FAILED) {
            impl->issues.push_back(failure->error);
        }
    }
    std::optional<ViewDesc> RenderSceneBridge::Camera(SceneId scene, ViewportId output, Extent2D extent) const
    {
        if (!impl->world || (extent.width == 0u) || (extent.height == 0u)) {
            return {};
        }
        for (const auto &actor : impl->world->GetActors()) {
            const auto *camera = actor->GetComponent<CameraComponent>();
            if (camera == nullptr) {
                continue;
            }
            const auto &data = camera->GetData();
            if (!std::isfinite(data.fov) || data.fov <= 0 || data.fov >= std::numbers::pi_v<float> || !std::isfinite(data.nearZ) ||
                !std::isfinite(data.farZ) || data.nearZ <= 0 || data.farZ <= data.nearZ) {
                return {};
            }
            const auto world = WorldMatrix(*actor);
            if (!std::isfinite(world.Determinant()) || std::abs(world.Determinant()) < 0.000001f) {
                return {};
            }
            const float scale = 1.f / std::tan(data.fov * 0.5f);
            ViewDesc    view;
            view.scene          = scene;
            view.output         = output;
            view.view           = world.Inverse();
            view.cameraPosition = {world.v[12], world.v[13], world.v[14]};
            view.projection     = Matrix4{};
            std::ranges::fill(view.projection.v, 0.f);
            view.projection.v[0]  = scale * float(extent.height) / float(extent.width);
            view.projection.v[5]  = scale;
            view.projection.v[10] = data.farZ / (data.nearZ - data.farZ);
            view.projection.v[11] = -1;
            view.projection.v[14] = data.nearZ * data.farZ / (data.nearZ - data.farZ);
            return view;
        }
        return {};
    }
} // namespace sky::aurora
