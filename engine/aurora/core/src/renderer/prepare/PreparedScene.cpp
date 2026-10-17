//
// Created on 2026/10/16.
//

#include "prepare/PreparedScene.h"

#include "prepare/RenderObjectPrepare.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace sky::aurora {
    namespace {
        using BindingKey = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, ResourceBindingSlot>;
        BindingKey Key(RenderEntityRef entity, ResourceBindingSlot slot)
        {
            return {entity.scene.index, entity.scene.generation, entity.entity.index, entity.entity.generation, slot};
        }
        using Bindings          = std::map<BindingKey, ResolvedResourceBinding>;
        constexpr uint64_t DRAW = 1, LIGHT = 2;

        bool Finite(const AABB &b)
        {
            return std::isfinite(b.min.x) && std::isfinite(b.min.y) && std::isfinite(b.min.z) && std::isfinite(b.max.x) && std::isfinite(b.max.y) &&
                   std::isfinite(b.max.z);
        }
        bool Compatible(const RenderResourceKey &selected, const RenderResourceKey &desired)
        {
            return selected.product == desired.product && selected.usageVariant == desired.usageVariant &&
                   selected.deviceEpoch == desired.deviceEpoch && selected.revision <= desired.revision;
        }
        std::shared_ptr<const ResourceVersionData> Select(const FrozenPrepareContext  &context,
                                                          const Bindings              &bindings,
                                                          RenderEntityRef              entity,
                                                          ResourceBindingSlot          slot,
                                                          const RenderResourceBinding &desired)
        {
            const auto it = bindings.find(Key(entity, slot));
            if (it == bindings.end())
                return {};
            const auto &binding = it->second;
            if (binding.desired != desired.resource || binding.subscriber.bindingRevision != desired.bindingRevision) {
                context.Fail("Resource snapshot does not match the frozen entity binding");
                return {};
            }
            return binding.selected;
        }
        void PrepareScene(const FrozenPrepareContext &context, SceneId scene, const Bindings &bindings)
        {
            context.Write<RenderObjectData, RenderTransform, RenderMeshBinding, RenderMaterialBinding, RenderVisibility, RenderProperties>(scene)
                .ForEachInvalidated(
                    [&](RenderEntityId id, const RenderObjectData &object) {
                        const auto &v = object.value;
                        return v.selectedMesh != Select(context, bindings, {scene, id}, ResourceBindingSlot::MESH, v.mesh) ||
                               v.selectedMaterial != Select(context, bindings, {scene, id}, ResourceBindingSlot::MATERIAL, v.material);
                    },
                    [&](RenderEntityId id, RenderObjectData::Value &object, const RenderTransform &transform, const RenderMeshBinding &mesh,
                        const RenderMaterialBinding &material, const RenderVisibility &visibility, const RenderProperties &properties) {
                        object                  = {transform.world, mesh.binding, material.binding, visibility.mask, properties.value};
                        object.selectedMesh     = Select(context, bindings, {scene, id}, ResourceBindingSlot::MESH, mesh.binding);
                        object.selectedMaterial = Select(context, bindings, {scene, id}, ResourceBindingSlot::MATERIAL, material.binding);
                        const auto &light       = properties.value.light;
                        if (light.intensity > 0 && light.range > 0) {
                            const Vector3 position{transform.world.v[12], transform.world.v[13], transform.world.v[14]};
                            object.lightBounds = AABB{position - Vector3(light.range), position + Vector3(light.range)};
                            if (!Finite(*object.lightBounds))
                                context.Fail("Point light bounds overflowed");
                        }
                        if (object.selectedMesh && object.selectedMesh->metadata.bounds) {
                            object.worldBounds = AABB::Transform(*object.selectedMesh->metadata.bounds, transform.world);
                            if (!Finite(*object.worldBounds))
                                context.Fail("Transformed object bounds overflowed");
                        }
                    });
        }

        // Homogeneous zero-to-one clip depth also supports reverse-Z; touching bounds remain visible.
        bool Visible(const AABB &bounds, const Matrix4 &clip)
        {
            std::array<bool, 6> outside{true, true, true, true, true, true};
            for (uint32_t corner = 0; corner != 8; ++corner) {
                const auto p = clip * Vector4{corner & 1 ? bounds.max.x : bounds.min.x, corner & 2 ? bounds.max.y : bounds.min.y,
                                              corner & 4 ? bounds.max.z : bounds.min.z, 1};
                const float e        = 1e-5f * std::max({1.0f, std::abs(p.x), std::abs(p.y), std::abs(p.z), std::abs(p.w)});
                const float planes[] = {p.x + p.w, p.w - p.x, p.y + p.w, p.w - p.y, p.z, p.w - p.z};
                for (size_t i = 0; i != outside.size(); ++i)
                    outside[i] = outside[i] && planes[i] < -e;
            }
            return std::none_of(outside.begin(), outside.end(), [](bool b) { return b; });
        }
        template <typename T>
        void Emit(const FrozenPrepareContext &context, uint64_t channel, const T &value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            context.Emit(channel, std::as_bytes(std::span{&value, 1}));
        }
        void ExtractView(const FrozenPrepareContext &context, const ViewDesc &view, const Matrix4 &clip)
        {
            context.Read<RenderObjectData>(view.scene).ForEach([&](RenderEntityId id, const RenderObjectData &data) {
                if (!data.valid) {
                    context.Fail("View extraction requires published scene data");
                    return;
                }
                const auto &object = data.value;
                if (!(object.visibilityMask & view.visibilityMask))
                    return;
                const Vector3 position{object.world.v[12], object.world.v[13], object.world.v[14]};
                const auto   &light = object.properties.light;
                if (object.lightBounds && Visible(*object.lightBounds, clip))
                    Emit(context, LIGHT, PreparedPointLight{{view.scene, id}, position, light});
                if (!object.selectedMesh || !object.selectedMaterial || !(object.properties.tags & view.drawTags) ||
                    (object.worldBounds && !Visible(*object.worldBounds, clip)))
                    return;
                Vector3 center = position;
                if (object.worldBounds)
                    center = object.worldBounds->min * 0.5f + object.worldBounds->max * 0.5f;
                const double x = double(center.x) - view.cameraPosition.x, y = double(center.y) - view.cameraPosition.y,
                             z              = double(center.z) - view.cameraPosition.z;
                const double       distance = std::sqrt(x * x + y * y + z * z);
                const auto        &lods     = object.selectedMesh->metadata.lodDistances;
                PreparedDrawPacket draw{{view.scene, id},
                                        object.world,
                                        {object.selectedMesh->resource, object.mesh.bindingRevision},
                                        {object.selectedMaterial->resource, object.material.bindingRevision}};
                draw.lod         = static_cast<uint32_t>(std::upper_bound(lods.begin(), lods.end(), distance) - lods.begin());
                draw.tags        = object.properties.tags;
                draw.worldBounds = object.worldBounds;
                Emit(context, DRAW, draw);
            });
        }
        auto ResourceOrder(const RenderResourceKey &key)
        {
            return std::tuple{key.product, key.usageVariant, key.deviceEpoch, key.revision};
        }
        auto EntityOrder(RenderEntityRef id)
        {
            return std::tuple{id.scene.index, id.scene.generation, id.entity.index, id.entity.generation};
        }
        bool DrawLess(const PreparedDrawPacket &a, const PreparedDrawPacket &b)
        {
            return std::tuple{a.tags, ResourceOrder(a.material.resource), ResourceOrder(a.mesh.resource), a.lod, a.subMesh, EntityOrder(a.entity)} <
                   std::tuple{b.tags, ResourceOrder(b.material.resource), ResourceOrder(b.mesh.resource), b.lod, b.subMesh, EntityOrder(b.entity)};
        }
    } // namespace

    std::variant<RendererError, PreparedSceneProgram> BuildPreparedSceneProgram(const FrozenCpuFrame                                       &frame,
                                                                                std::shared_ptr<const std::vector<ResolvedResourceBinding>> resources,
                                                                                size_t                   rowsPerChunk,
                                                                                std::span<const SceneId> paused)
    {
        if (!resources || !rowsPerChunk || rowsPerChunk > 65536)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid scene preparation inputs or chunk size"};
        auto bindings = std::make_shared<Bindings>();
        for (const auto &binding : *resources) {
            const auto &s = binding.subscriber;
            if (!s.entity.scene.IsValid() || !s.entity.entity.IsValid() ||
                (s.slot != ResourceBindingSlot::MESH && s.slot != ResourceBindingSlot::MATERIAL) ||
                !bindings->emplace(Key(s.entity, s.slot), binding).second ||
                (binding.selected &&
                 (!Compatible(binding.selected->resource, binding.desired) || !IsValidRenderResourceMetadata(binding.selected->metadata))))
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid, duplicate or incompatible resource snapshot"};
        }
        PreparedSceneProgram program;
        program.frame.packetSequence = frame.Sequence();
        program.frame.resources      = std::move(resources);
        struct Work {
            SceneId                 scene;
            std::optional<ViewDesc> view;
            Matrix4                 clip;
        };
        std::vector<Work>                                 work;
        std::map<std::pair<uint32_t, uint32_t>, SystemId> producers;
        for (SceneId scene : frame.Scenes()) {
            if (std::find(paused.begin(), paused.end(), scene) != paused.end())
                continue;
            const SystemId id = work.size() + 1;
            program.sceneSystems.push_back({id, scene});
            auto system = DescribeRenderObjectPrepare(id, scene);
            program.work.systems.push_back(std::move(system));
            program.work.partitions.push_back({id, scene, rowsPerChunk});
            producers.emplace(std::pair{scene.index, scene.generation}, id);
            work.push_back({scene, {}, {}});
        }
        for (const auto &input : frame.Views()) {
            const auto &view = input.view;
            if (!producers.contains({view.desc.scene.index, view.desc.scene.generation}))
                continue;
            const SystemId id = work.size() + 1;
            SystemDesc     system{id, "ExtractView"};
            system.accesses = {ComponentAccess<RenderObjectData>("scene", SceneAccessOwner(view.desc.scene), AccessMode::READ)};
            system.after    = {producers.at({view.desc.scene.index, view.desc.scene.generation})};
            program.work.systems.push_back(std::move(system));
            program.work.partitions.push_back({id, view.desc.scene, rowsPerChunk});
            program.work.outputs.push_back({id, rowsPerChunk * 2, rowsPerChunk * (sizeof(PreparedDrawPacket) + sizeof(PreparedPointLight)), 0});
            program.frame.views.push_back({view.id, view.desc, input.sceneRevision, {}});
            program.viewSystems.push_back(id);
            const auto clip = view.desc.projection * view.desc.view;
            if (!std::all_of(std::begin(clip.v), std::end(clip.v), [](float v) { return std::isfinite(v); }))
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "View projection overflows"};
            work.push_back({view.desc.scene, view.desc, clip});
        }
        program.work.kernel = [work = std::move(work), bindings = std::move(bindings)](const FrozenPrepareContext &context) {
            const auto &item = work[context.Id() - 1];
            if (item.view)
                ExtractView(context, *item.view, item.clip);
            else
                PrepareScene(context, item.scene, *bindings);
        };
        return program;
    }

    std::variant<RendererError, std::shared_ptr<const PreparedFrame>> CollectPreparedScene(const PreparedSceneProgram &program,
                                                                                           const PrepareResult        &result)
    {
        if (!result.Succeeded() || (!program.frame.views.empty() && !result.products) ||
            (result.products && result.products->deferred.sourceSequence != program.frame.packetSequence))
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Preparation failed or belongs to a different frame"};
        auto frame = std::make_shared<PreparedFrame>(program.frame);
        if (result.products) {
            for (const auto &record : result.products->records) {
                const auto it = std::lower_bound(program.viewSystems.begin(), program.viewSystems.end(), record.origin.system);
                if (it == program.viewSystems.end() || *it != record.origin.system)
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Unknown view output producer"};
                auto      &view  = frame->views[it - program.viewSystems.begin()];
                const auto bytes = result.products->Read(record);
                if (record.channel == DRAW && bytes.size() == sizeof(PreparedDrawPacket)) {
                    PreparedDrawPacket draw;
                    std::memcpy(&draw, bytes.data(), bytes.size());
                    view.draws.push_back(draw);
                } else if (record.channel == LIGHT && bytes.size() == sizeof(PreparedPointLight)) {
                    PreparedPointLight light;
                    std::memcpy(&light, bytes.data(), bytes.size());
                    view.lights.push_back(light);
                } else {
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid view output schema"};
                }
            }
        }
        for (auto &view : frame->views) {
            std::sort(view.draws.begin(), view.draws.end(), DrawLess);
            std::sort(view.lights.begin(), view.lights.end(),
                      [](const auto &a, const auto &b) { return EntityOrder(a.entity) < EntityOrder(b.entity); });
        }
        return std::shared_ptr<const PreparedFrame>(std::move(frame));
    }
} // namespace sky::aurora
