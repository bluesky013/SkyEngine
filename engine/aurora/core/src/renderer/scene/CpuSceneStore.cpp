#include "scene/CpuSceneStore.h"
#include "scene/FrozenCpuFrame.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <condition_variable>
#include <mutex>
#include <set>
#include <type_traits>

namespace sky::aurora {
    namespace {
        enum class ObjectKind { SCENE, ENTITY, VIEW, VIEWPORT };
        struct ObjectKey {
            ObjectKind kind;
            uint32_t   parent                               = 0;
            uint32_t   parentGeneration                     = 0;
            uint32_t   index                                = 0;
            auto       operator<=>(const ObjectKey &) const = default;
        };
        struct Lifetime {
            uint32_t generation = 0;
            bool     alive      = false;
            bool     pending    = false;
            uint64_t revision   = 0;
        };
        using Lifetimes = std::map<ObjectKey, Lifetime>;

        ObjectKey Key(SceneId id)
        {
            return {ObjectKind::SCENE, 0, 0, id.index};
        }
        ObjectKey Key(ViewId id)
        {
            return {ObjectKind::VIEW, 0, 0, id.index};
        }
        ObjectKey Key(ViewportId id)
        {
            return {ObjectKind::VIEWPORT, 0, 0, id.index};
        }
        ObjectKey Key(RenderEntityRef id)
        {
            return {ObjectKind::ENTITY, id.scene.index, id.scene.generation, id.entity.index};
        }
        template <typename T>
        uint32_t Generation(T id)
        {
            return id.generation;
        }
        uint32_t Generation(RenderEntityRef id)
        {
            return id.entity.generation;
        }
        template <typename T>
        bool Valid(T id)
        {
            return id.IsValid();
        }
        bool Valid(RenderEntityRef id)
        {
            return id.scene.IsValid() && id.entity.IsValid();
        }

        bool Finite(const Matrix4 &matrix)
        {
            return std::all_of(std::begin(matrix.v), std::end(matrix.v), [](float v) { return std::isfinite(v); });
        }

        template <typename T>
        bool ValidCommand(const T &command)
        {
            if (!Valid(command.id)) {
                return false;
            }
            if constexpr (requires { command.revision; }) {
                if (command.revision == 0) {
                    return false;
                }
            }
            if constexpr (std::is_same_v<T, CreateRenderEntity> || std::is_same_v<T, UpdateRenderEntity>) {
                return IsValidRenderEntityDesc(command.desc);
            } else if constexpr (std::is_same_v<T, CreateView> || std::is_same_v<T, UpdateView>) {
                const auto &desc = command.desc;
                return Valid(desc.scene) && Valid(desc.output) && Finite(desc.view) && Finite(desc.projection) &&
                       std::isfinite(desc.cameraPosition.x) && std::isfinite(desc.cameraPosition.y) && std::isfinite(desc.cameraPosition.z);
            } else if constexpr (std::is_same_v<T, CreateViewport>) {
                const auto &desc = command.desc;
                return desc.format > PixelFormat::UNDEFINED && desc.format < PixelFormat::MAX &&
                       ((desc.kind == ViewportKind::OFFSCREEN && desc.nativeWindow == 0) ||
                        (desc.kind == ViewportKind::NATIVE_WINDOW && desc.nativeWindow != 0));
            }
            return true;
        }

        template <typename T>
        constexpr bool IsCreate = std::is_same_v<T, CreateScene> || std::is_same_v<T, CreateRenderEntity> || std::is_same_v<T, CreateView> ||
                                  std::is_same_v<T, CreateViewport>;
        template <typename T>
        constexpr bool IsDestroy = std::is_same_v<T, DestroyScene> || std::is_same_v<T, DestroyRenderEntity> || std::is_same_v<T, DestroyView> ||
                                   std::is_same_v<T, DestroyViewport>;

        class Preflight {
        public:
            explicit Preflight(const Lifetimes &current) : current(current)
            {
            }

            template <typename Id>
            bool Alive(Id id) const
            {
                const auto life = Lookup(Key(id));
                return life.alive && life.generation == Generation(id);
            }

            bool CheckDeferred(const DeferredEntityCommand &command, size_t index, PacketApplyResult &result)
            {
                return std::visit(
                    [&](const auto &action) {
                        using T = std::decay_t<decltype(action)>;
                        if (command.expectedRevision.has_value() == std::is_same_v<T, CreateRenderEntity>) {
                            result.error = RendererError{RendererErrorCode::INVALID_PACKET, "Invalid deferred revision precondition"};
                            return false;
                        }
                        if (command.expectedRevision) {
                            const auto found = current.find(Key(action.id));
                            if (found == current.end() || !found->second.alive || found->second.generation != action.id.entity.generation) {
                                result.skipped.push_back({index, CommandSkipReason::STALE_HANDLE, CommandSource::DEFERRED});
                                return false;
                            }
                            if (found->second.revision != *command.expectedRevision) {
                                result.skipped.push_back({index, CommandSkipReason::STALE_REVISION, CommandSource::DEFERRED});
                                return false;
                            }
                        }
                        return Check(RenderCommand{action}, index, result, CommandSource::DEFERRED);
                    },
                    command.action);
            }

            bool Check(const RenderCommand &command, size_t index, PacketApplyResult &result, CommandSource source = CommandSource::PACKET)
            {
                return std::visit(
                    [&](const auto &cmd) {
                        using T = std::decay_t<decltype(cmd)>;
                        if constexpr (std::is_same_v<T, ResourceCpuReady>) {
                            result.error = RendererError{RendererErrorCode::UNSUPPORTED, "Resource ingestion is not connected to the CPU store"};
                            return false;
                        } else {
                            if (!ValidCommand(cmd)) {
                                result.error = RendererError{RendererErrorCode::INVALID_PACKET, "Invalid command at index " + std::to_string(index)};
                                return false;
                            }
                            const auto skip = [&](CommandSkipReason reason) {
                                result.skipped.push_back({index, reason, source});
                                return false;
                            };
                            if constexpr (std::is_same_v<decltype(cmd.id), RenderEntityRef>) {
                                if (!Alive(cmd.id.scene)) {
                                    return skip(CommandSkipReason::STALE_HANDLE);
                                }
                            }
                            if constexpr (std::is_same_v<T, CreateView> || std::is_same_v<T, UpdateView>) {
                                if (!Alive(cmd.desc.scene) || !Alive(cmd.desc.output)) {
                                    return skip(CommandSkipReason::STALE_HANDLE);
                                }
                            }
                            auto life = Lookup(Key(cmd.id));
                            if constexpr (IsCreate<T>) {
                                if (life.pending) {
                                    return skip(CommandSkipReason::RETIREMENT_PENDING);
                                }
                                if (life.alive || Generation(cmd.id) <= life.generation) {
                                    return skip(CommandSkipReason::STALE_HANDLE);
                                }
                                life = {Generation(cmd.id), true, false, 0};
                            } else {
                                if (!life.alive || life.generation != Generation(cmd.id)) {
                                    return skip(CommandSkipReason::STALE_HANDLE);
                                }
                                if constexpr (IsDestroy<T>) {
                                    life.alive   = false;
                                    life.pending = true;
                                    result.retirements.emplace_back(cmd.id);
                                } else {
                                    auto revision = life.revision;
                                    if constexpr (std::is_same_v<T, UpdateRenderEntity>) {
                                        // Host input is newer relative to boundary entry, not a speculative deferred revision.
                                        if (source == CommandSource::PACKET && deferredUpdated.contains(Key(cmd.id)))
                                            revision = current.at(Key(cmd.id)).revision;
                                    }
                                    if (cmd.revision <= revision) {
                                        return skip(CommandSkipReason::STALE_REVISION);
                                    }
                                    life.revision = cmd.revision;
                                    if constexpr (std::is_same_v<T, UpdateRenderEntity>) {
                                        if (source == CommandSource::DEFERRED)
                                            deferredUpdated.insert(Key(cmd.id));
                                        else
                                            deferredUpdated.erase(Key(cmd.id));
                                    }
                                }
                            }
                            changed[Key(cmd.id)] = life;
                            return true;
                        }
                    },
                    command);
            }

            Lifetimes changed;

        private:
            Lifetime Lookup(ObjectKey key) const
            {
                if (const auto it = changed.find(key); it != changed.end()) {
                    return it->second;
                }
                const auto it = current.find(key);
                return it == current.end() ? Lifetime{} : it->second;
            }
            const Lifetimes    &current;
            std::set<ObjectKey> deferredUpdated;
        };
    } // namespace

    struct CpuSceneStore::Storage {
        std::mutex                                         freezeMutex;
        std::condition_variable                            unfrozen;
        bool                                               frozen = false;
        Lifetimes                                          lifetimes;
        std::map<uint32_t, std::unique_ptr<CpuSceneState>> scenes;
        std::map<uint32_t, CpuViewState>                   views;
        std::map<uint32_t, CpuViewportState>               viewports;
        uint64_t                                           lastSequence        = 0;
        uint64_t                                           lastAppliedSequence = 0;
        bool                                               failed              = false;

        bool ApplyCommand(const RenderCommand &command, uint64_t sequence)
        {
            return std::visit(
                [&](const auto &cmd) -> bool {
                    using T = std::decay_t<decltype(cmd)>;
                    if constexpr (std::is_same_v<T, CreateScene>) {
                        std::unique_ptr<CpuSceneState> scene = std::make_unique<CpuSceneState>();
                        scene->id                  = cmd.id;
                        scene->desc                = cmd.desc;
                        scene->lastChangedSequence = sequence;
                        scene->registry.Pool<RenderTransform>();
                        scene->registry.Pool<RenderMeshBinding>();
                        scene->registry.Pool<RenderMaterialBinding>();
                        scene->registry.Pool<RenderVisibility>();
                        scene->registry.Pool<RenderProperties>();
                        scene->registry.Pool<RenderObjectData>();
                        scenes.emplace(cmd.id.index, std::move(scene));
                    } else if constexpr (std::is_same_v<T, DestroyScene>) {
                        scenes.erase(cmd.id.index);
                    } else if constexpr (std::is_same_v<T, CreateRenderEntity> || std::is_same_v<T, UpdateRenderEntity> ||
                                         std::is_same_v<T, DestroyRenderEntity>) {
                        CpuSceneState &scene = *scenes.at(cmd.id.scene.index);
                        if constexpr (std::is_same_v<T, CreateRenderEntity>) {
                            const auto entity = scene.registry.CreateEntity();
                            if (!IsValidEntity(entity)) {
                                return false;
                            }
                            if (!scene.registry.Add<RenderTransform>(entity, {cmd.desc.world}) ||
                                !scene.registry.Add<RenderMeshBinding>(entity, {cmd.desc.mesh}) ||
                                !scene.registry.Add<RenderMaterialBinding>(entity, {cmd.desc.material}) ||
                                !scene.registry.Add<RenderVisibility>(entity, {cmd.desc.visibilityMask}) ||
                                !scene.registry.Add<RenderProperties>(entity, {cmd.desc.properties}) ||
                                !scene.registry.Add<RenderObjectData>(entity, {})) {
                                scene.registry.DestroyEntity(entity);
                                return false;
                            }
                            scene.entities.emplace(cmd.id.entity.index, RenderEntityState{cmd.id.entity, entity, 0});
                        } else {
                            auto &entity = scene.entities.at(cmd.id.entity.index);
                            if constexpr (std::is_same_v<T, DestroyRenderEntity>) {
                                scene.registry.DestroyEntity(entity.entity);
                                scene.entities.erase(cmd.id.entity.index);
                            } else {
                                scene.registry.Get<RenderTransform>(entity.entity)->world         = cmd.desc.world;
                                scene.registry.Get<RenderMeshBinding>(entity.entity)->binding     = cmd.desc.mesh;
                                scene.registry.Get<RenderMaterialBinding>(entity.entity)->binding = cmd.desc.material;
                                scene.registry.Get<RenderVisibility>(entity.entity)->mask         = cmd.desc.visibilityMask;
                                scene.registry.Get<RenderProperties>(entity.entity)->value        = cmd.desc.properties;
                                entity.revision                                                   = cmd.revision;
                                scene.registry.Get<RenderObjectData>(entity.entity)->valid        = false;
                            }
                        }
                        scene.lastChangedSequence = sequence;
                    } else if constexpr (std::is_same_v<T, CreateView>) {
                        views.emplace(cmd.id.index, CpuViewState{cmd.id, cmd.desc, 0});
                    } else if constexpr (std::is_same_v<T, UpdateView>) {
                        views.at(cmd.id.index) = {cmd.id, cmd.desc, cmd.revision};
                    } else if constexpr (std::is_same_v<T, DestroyView>) {
                        views.erase(cmd.id.index);
                    } else if constexpr (std::is_same_v<T, CreateViewport>) {
                        viewports.emplace(cmd.id.index, CpuViewportState{cmd.id, cmd.desc, 0});
                    } else if constexpr (std::is_same_v<T, ResizeViewport>) {
                        auto &viewport       = viewports.at(cmd.id.index);
                        viewport.desc.extent = cmd.extent;
                        viewport.revision    = cmd.revision;
                    } else if constexpr (std::is_same_v<T, DestroyViewport>) {
                        viewports.erase(cmd.id.index);
                    }
                    return true;
                },
                command);
        }
    };

    const RenderEntityState *CpuSceneState::FindEntity(RenderEntityId entity) const
    {
        const auto it = entities.find(entity.index);
        return it != entities.end() && it->second.id == entity ? &it->second : nullptr;
    }

    CpuSceneStore::CpuSceneStore() : storage(std::make_shared<Storage>())
    {
    }
    CpuSceneStore::~CpuSceneStore() = default;

    PacketApplyResult CpuSceneStore::Apply(const FramePacket &packet, const DeferredSceneBatch *deferred, bool acceptResources)
    {
        PacketApplyResult result;
        result.sequence = packet.sequence;
        if (IsFrozen()) {
            result.error = RendererError{RendererErrorCode::NOT_READY, "CPU frame borrowers are still active"};
            return result;
        }
        if (storage->failed) {
            result.error = RendererError{RendererErrorCode::NOT_READY, "CPU store failed; reconstruction is required"};
            return result;
        }
        if (packet.sequence == 0 || packet.sequence <= storage->lastSequence) {
            result.error = RendererError{RendererErrorCode::INVALID_PACKET, "Packet sequence must increase and be nonzero"};
            return result;
        }
        storage->lastSequence = packet.sequence;
        if (deferred && (deferred->sourceSequence == 0 || deferred->sourceSequence != storage->lastAppliedSequence)) {
            result.error = RendererError{RendererErrorCode::INVALID_PACKET, "Deferred batch does not match the latest applied frame"};
            return result;
        }
        if (!std::isfinite(packet.timeSeconds) || !std::isfinite(packet.deltaSeconds) || packet.deltaSeconds < 0 ||
            (!acceptResources && (!packet.extensions.empty() || !packet.featureCommands.empty()))) {
            result.error = RendererError{packet.extensions.empty() && packet.featureCommands.empty() ? RendererErrorCode::INVALID_PACKET
                                                                                                     : RendererErrorCode::UNSUPPORTED,
                                         "Invalid frame timing or unsupported frame extensions"};
            return result;
        }
        Preflight         preflight(storage->lifetimes);
        std::vector<bool> applyDeferred(deferred ? deferred->commands.size() : 0);
        std::vector<bool> apply(packet.commands.size());
        for (size_t i = 0; i < applyDeferred.size(); ++i) {
            applyDeferred[i] = preflight.CheckDeferred(deferred->commands[i], i, result);
            if (result.error) {
                result.retirements.clear();
                return result;
            }
        }
        for (size_t i = 0; i < packet.commands.size(); ++i) {
            if (acceptResources && std::holds_alternative<ResourceCpuReady>(packet.commands[i]))
                continue;
            apply[i] = preflight.Check(packet.commands[i], i, result);
            if (result.error) {
                result.retirements.clear();
                return result;
            }
        }
        const auto applyCommand = [&](const RenderCommand &command) {
            if (storage->ApplyCommand(command, packet.sequence))
                return true;
            storage->failed = true;
            result.error    = RendererError{RendererErrorCode::INTERNAL_ERROR, "Scene entity capacity exhausted"};
            result.retirements.clear();
            result.changedScenes.clear();
            return false;
        };
        for (size_t i = 0; i < applyDeferred.size(); ++i) {
            if (applyDeferred[i] &&
                !std::visit([&](const auto &action) { return applyCommand(RenderCommand{action}); }, deferred->commands[i].action))
                return result;
        }
        for (size_t i = 0; i < packet.commands.size(); ++i) {
            if (apply[i] && !applyCommand(packet.commands[i]))
                return result;
        }
        for (const auto &[key, life] : preflight.changed) {
            storage->lifetimes.insert_or_assign(key, life);
        }
        for (const auto &retirement : result.retirements) {
            if (const auto *scene = std::get_if<SceneId>(&retirement)) {
                std::erase_if(storage->lifetimes, [&](const auto &entry) {
                    return entry.first.kind == ObjectKind::ENTITY && entry.first.parent == scene->index &&
                           entry.first.parentGeneration == scene->generation;
                });
            }
        }
        for (const auto &[index, scene] : storage->scenes) {
            if (scene->lastChangedSequence == packet.sequence) {
                result.changedScenes.push_back(scene->id);
            }
        }
        storage->lastAppliedSequence = packet.sequence;
        if (deferred) {
            result.deferredSequence = deferred->sourceSequence;
            result.deferredCommands = deferred->commands.size();
        }
        return result;
    }

    bool CpuSceneStore::AcknowledgeRetirement(const CpuRetirement &retirement)
    {
        if (storage->failed || IsFrozen()) {
            return false;
        }
        return std::visit(
            [&](auto id) {
                const auto it = storage->lifetimes.find(Key(id));
                if (it == storage->lifetimes.end() || !it->second.pending || it->second.generation != Generation(id)) {
                    return false;
                }
                it->second.pending = false;
                return true;
            },
            retirement);
    }

    const CpuSceneState *CpuSceneStore::FindScene(SceneId id) const
    {
        const auto it = storage->scenes.find(id.index);
        return !storage->failed && it != storage->scenes.end() && it->second->id == id ? it->second.get() : nullptr;
    }

    const CpuViewState *CpuSceneStore::FindView(ViewId id) const
    {
        const auto it = storage->views.find(id.index);
        return !storage->failed && it != storage->views.end() && it->second.id == id ? &it->second : nullptr;
    }

    const CpuViewportState *CpuSceneStore::FindViewport(ViewportId id) const
    {
        const auto it = storage->viewports.find(id.index);
        return !storage->failed && it != storage->viewports.end() && it->second.id == id ? &it->second : nullptr;
    }

    CpuPrepareSelection CpuSceneStore::SelectPrepareInputs() const
    {
        CpuPrepareSelection                     selection;
        std::set<std::pair<uint32_t, uint32_t>> scenes;
        if (!storage->failed) {
            for (const auto &[index, view] : storage->views) {
                const auto *output = FindViewport(view.desc.output);
                const auto  extent = output ? (output->surface ? output->surface->extent : output->desc.extent) : Extent2D{};
                if (FindScene(view.desc.scene) && output && extent.width != 0 && extent.height != 0) {
                    selection.views.push_back(view.id);
                    scenes.emplace(view.desc.scene.index, view.desc.scene.generation);
                }
            }
        }
        for (const auto &[index, generation] : scenes) {
            selection.scenes.push_back({index, generation});
        }
        return selection;
    }

    bool CpuSceneStore::SetViewportSurface(ViewportId id, const SurfaceConfiguration &surface)
    {
        std::lock_guard lock(storage->freezeMutex);
        const auto      found = storage->viewports.find(id.index);
        if (storage->frozen || found == storage->viewports.end() || found->second.id != id)
            return false;
        found->second.surface = surface;
        return true;
    }

    bool CpuSceneStore::IsFailed() const
    {
        return storage->failed;
    }

    std::shared_ptr<const FrozenCpuFrame> CpuSceneStore::Freeze()
    {
        std::unique_lock lock(storage->freezeMutex);
        if (storage->failed || storage->frozen || storage->lastAppliedSequence == 0) {
            return {};
        }
        auto frame       = std::shared_ptr<FrozenCpuFrame>(new FrozenCpuFrame());
        frame->sequence  = storage->lastAppliedSequence;
        frame->selection = SelectPrepareInputs();
        for (const auto &[index, scene] : storage->scenes) {
            frame->scenes.push_back(scene.get());
        }
        for (auto id : frame->selection.views) {
            const auto &view     = *FindView(id);
            const auto &output   = *FindViewport(view.desc.output);
            const auto  extent   = output.surface ? output.surface->extent : output.desc.extent;
            const auto  format   = output.surface ? output.surface->format : output.desc.format;
            const auto  revision = output.surface ? output.surface->revision : output.revision;
            frame->views.push_back({view, extent, format, revision, FindScene(view.desc.scene)->lastChangedSequence});
        }
        struct Lease {
            explicit Lease(std::shared_ptr<Storage> owner) : owner(std::move(owner))
            {
            }
            ~Lease()
            {
                {
                    std::lock_guard lock(owner->freezeMutex);
                    owner->frozen = false;
                }
                owner->unfrozen.notify_all();
            }
            std::shared_ptr<Storage> owner;
        };
        frame->lease    = std::make_shared<Lease>(storage);
        storage->frozen = true;
        return frame;
    }

    bool CpuSceneStore::IsFrozen() const
    {
        std::lock_guard lock(storage->freezeMutex);
        return storage->frozen;
    }

    void CpuSceneStore::WaitUntilUnfrozen() const
    {
        std::unique_lock lock(storage->freezeMutex);
        storage->unfrozen.wait(lock, [&] { return !storage->frozen; });
    }

} // namespace sky::aurora
