#include <animation/world/AnimationWorldHost.h>

#include <algorithm>
#include <animation/IAnimationClipSource.h>
#include <animation/IAnimationSkeletonSource.h>
#include <animation/assets/GraphAsset.h>
#include <animation/core/AnimationPlayer.h>
#include <animation/core/AnimationUtils.h>
#include <animation/graph/GraphInstance.h>
#include <animation/world/AnimationComponent.h>
#include <cmath>
#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <unordered_map>
#include <unordered_set>

namespace sky::animation {
    Uuid AnimationWorldHost::GetComponentType() const
    {
        return TypeInfo<AnimationComponent>::RegisteredId();
    }
    namespace {
        AnimationStatus Stale()
        {
            return {AnimationError::StaleGeneration, "Animation instance generation is stale"};
        }
        bool SameBinding(const AnimationComponentData &a, const AnimationComponentData &b)
        {
            return a.mode == b.mode && a.skeleton == b.skeleton && a.clip == b.clip && a.graph == b.graph &&
                   (a.mode != AnimationMode::Graph || a.parameters == b.parameters);
        }
        struct BoundAnimation {
            std::shared_ptr<Asset<GraphAsset>>                graphAsset;
            std::vector<AnimationSourceResult<AnimationClip>> graphClips;

            AnimationSourceResult<Skeleton>      rig;
            AnimationSourceResult<AnimationClip> clip;
            // Plans and players must die before their provider resource and code leases.
            std::unique_ptr<GraphInstance> graph;
            AnimationSequencePlayer        player;
            AnimPose                       pose;
            std::vector<Matrix4>           models;
        };
        struct Instance {
            std::shared_ptr<Asset<GraphAsset>>                                  graphRequest;
            std::vector<std::pair<Uuid, AnimationSourceRequest<AnimationClip>>> graphRequests;

            bool                                  seen = false;
            AnimationComponentData                config;
            Uuid                                  attachment;
            Uuid                                  generation = Uuid::Create();
            AnimationSourceRequest<Skeleton>      rigRequest;
            AnimationSourceRequest<AnimationClip> clipRequest;
            std::unique_ptr<BoundAnimation>       bound;
            AnimationStatus                       diagnostic;
            AnimationSourceState                  state        = AnimationSourceState::Pending;
            uint64_t                              poseRevision = 0;
            bool                                  pending      = false;
            bool                                  retained     = false;
            float                                 rate         = 1.f;
        };
        class System;
        struct Registry {
            std::shared_ptr<const void>                code;
            std::unordered_map<const void *, System *> systems;
            bool                                       active = true;
        };
        class System final : public IWorldSubSystem {
        public:
            explicit System(std::shared_ptr<Registry> owner) : registry(std::move(owner))
            {
            }
            ~System() override
            {
                Detach();
            }
            void OnAttachToWorld(World &value) override
            {
                world = &value;
                epoch = value.GetRuntimeId();
                registry->systems.emplace(world, this);
            }
            void OnDetachFromWorld(World &) override
            {
                Detach();
            }
            void StartSimulation() override
            {
                running = true;
            }
            void StopSimulation() override
            {
                running = false;
                instances.clear();
            }
            void Drain()
            {
                instances.clear();
                running = false;
            }
            void Tick(float delta) override
            {
                if (!world || !registry->active) {
                    return;
                }
                if (epoch != world->GetRuntimeId()) {
                    epoch = world->GetRuntimeId();
                    instances.clear();
                }
                const auto *clips     = Interface<IAnimationClipSource>::Get()->GetApi();
                const auto *rigs      = Interface<IAnimationSkeletonSource>::Get()->GetApi();
                const auto  nextClips = clips ? clips->GetChangeSerial() : 0;
                const auto  nextRigs  = rigs ? rigs->GetChangeSerial() : 0;
                const auto  nextGraph = AssetManager::Get()->GetProductChangeSerial();
                const bool  reload =
                    nextClips != clipSerial || nextRigs != rigSerial || clips != clipProvider || rigs != rigProvider || graphSerial != nextGraph;
                graphSerial  = nextGraph;
                clipProvider = clips;
                rigProvider  = rigs;
                clipSerial   = nextClips;
                rigSerial    = nextRigs;
                for (auto &[id, instance] : instances) {
                    instance->seen = false;
                }
                for (const auto &actor : world->GetActors()) {
                    const auto *component = actor->GetComponent<AnimationComponent>();
                    if (!component || !component->GetAttachment()) {
                        continue;
                    }
                    const auto id = actor->GetUuid();

                    auto       &slot   = instances[id];
                    const auto &config = component->GetData();
                    if (!slot || slot->attachment != component->GetAttachment() || !SameBinding(slot->config, config)) {
                        slot             = std::make_unique<Instance>();
                        slot->config     = config;
                        slot->rate       = config.playRate;
                        slot->attachment = component->GetAttachment();
                        Request(*slot);
                    } else if (reload) {
                        Request(*slot);
                    }
                    auto &instance = *slot;
                    instance.seen  = true;
                    if (instance.config.playRate != config.playRate) {
                        instance.rate = config.playRate;
                    }
                    instance.config = config;
                    if (Poll(instance, running ? delta : 0.f, running)) {
                        continue;
                    }
                    if (!instance.bound || instance.retained) {
                        continue;
                    }
                    auto &bound  = *instance.bound;
                    auto  status = AdvanceBound(bound, instance.rate, config.loop, delta, running);
                    if (status) {
                        status = Evaluate(instance);
                    }
                    instance.diagnostic = status;
                    instance.state      = status ? AnimationSourceState::Ready : AnimationSourceState::Error;
                }
                std::erase_if(instances, [&](const auto &entry) { return !entry.second->seen; });
            }
            Instance *Find(const Uuid &actor) const
            {
                if (!world || !registry->active || epoch != world->GetRuntimeId()) {
                    return nullptr;
                }
                const auto found = instances.find(actor);
                if (found == instances.end()) {
                    return nullptr;
                }
                auto       *owner     = world->GetActorByUuid(actor);
                const auto *component = owner ? owner->GetComponent<AnimationComponent>() : nullptr;
                auto       *instance  = found->second.get();
                if (!component || component->GetAttachment() != instance->attachment || !SameBinding(component->GetData(), instance->config)) {
                    return nullptr;
                }
                return instance;
            }
            AnimationInstanceStatus Status(const Uuid &actor, bool includeDebug = true) const
            {
                AnimationInstanceStatus result;
                result.handle        = {world, epoch, actor, {}};
                const auto *instance = Find(actor);
                if (!instance) {
                    result.diagnostic = Stale();
                    result.state      = AnimationSourceState::Error;
                    return result;
                }
                result.handle.generation = instance->generation;
                result.state             = instance->state;
                result.diagnostic        = instance->diagnostic;
                result.retainedVersion   = instance->retained;
                result.poseRevision      = instance->poseRevision;
                result.rate              = instance->rate;
                if (instance->bound) {
                    const auto &bound = *instance->bound;
                    result.skeleton   = bound.rig.version;
                    if (bound.graph) {
                        result.animation = {bound.graphAsset->GetUuid(), bound.graphAsset->GetInstanceId(), bound.graphAsset->GetRevision(), 0};
                        result.playing   = bound.graph->IsPlaying();
                        if (includeDebug) {
                            const auto debug          = bound.graph->Debug();
                            result.activeState        = debug.activeState;
                            result.destinationState   = debug.destinationState;
                            result.transition         = debug.transition;
                            result.transitionProgress = debug.progress;
                            for (const auto &node : debug.nodes) {
                                result.nodes.push_back({node.node, node.sampleTime, node.elapsed, node.active});
                            }
                            const auto &parameters = bound.graphAsset->Data().graph.definition.parameters;
                            const auto &values     = bound.graph->GetParameters();
                            for (size_t i = 0; i < parameters.size(); ++i) {
                                result.parameters.push_back({parameters[i].id, parameters[i].name, values[i]});
                            }
                        }
                    } else {
                        result.animation = bound.clip.version;
                        result.playing   = bound.player.IsPlaying();
                        result.time      = bound.player.GetCurrentTime();
                        result.duration  = bound.clip.resource->GetDuration();
                    }
                }
                return result;
            }
            AnimationStatus Control(const AnimationInstanceHandle &handle, AnimationControlOp op, float value)
            {
                auto *instance = Find(handle.actor);
                if (!instance || handle.worldEpoch != epoch || handle.generation != instance->generation) {
                    return Stale();
                }
                if (op == AnimationControlOp::Reload) {
                    Request(*instance);
                    return {};
                }
                if (!instance->bound) {
                    return {AnimationError::InvalidPlayback, "Animation resources are not ready"};
                }
                if (auto *graph = instance->bound->graph.get()) {
                    switch (op) {
                    case AnimationControlOp::Play: graph->SetPlaying(true); return {};
                    case AnimationControlOp::Pause: graph->SetPlaying(false); return {};
                    case AnimationControlOp::Stop: graph->Stop(); return Evaluate(*instance);
                    case AnimationControlOp::Seek: return {AnimationError::Unsupported, "Graph Seek is unsupported"};
                    case AnimationControlOp::SetRate: {
                        const auto status = graph->SetRate(value);
                        if (status) {
                            instance->rate = value;
                        }
                        return status;
                    }
                    default: return {AnimationError::Unsupported, "Unknown animation control"};
                    }
                }
                auto           &player = instance->bound->player;
                AnimationStatus result;
                switch (op) {
                case AnimationControlOp::Play: result = player.SetPlaying(true); break;
                case AnimationControlOp::Pause: result = player.SetPlaying(false); break;
                case AnimationControlOp::Stop:
                    player.Stop();
                    result = Evaluate(*instance);
                    break;
                case AnimationControlOp::Seek:
                    result = player.Seek(value);
                    if (result) {
                        result = Evaluate(*instance);
                    }
                    break;
                case AnimationControlOp::SetRate:
                    result = player.SetPlayRate(value);
                    if (result) {
                        instance->rate = value;
                    }
                    break;
                default: result = {AnimationError::Unsupported, "Unknown animation control"}; break;
                }
                return result;
            }
            AnimationStatus SetParameter(const AnimationInstanceHandle &handle, const Uuid &id, const AnimationParam &value)
            {
                auto *instance = Find(handle.actor);
                if (!instance || handle.worldEpoch != epoch || handle.generation != instance->generation) {
                    return Stale();
                }
                if (!instance->bound || !instance->bound->graph) {
                    return {AnimationError::Unsupported, "Instance has no ready Graph"};
                }
                const auto status = instance->bound->graph->SetParameter(id, value);
                return status ? Evaluate(*instance) : status;
            }
            aurora::SkeletalPoseResult Describe(const Uuid &actor) const
            {
                const auto                 status = Status(actor, false);
                aurora::SkeletalPoseResult result;
                result.instance        = {world, epoch, actor, status.handle.generation};
                result.skeleton        = {status.skeleton.asset, status.skeleton.generation, status.skeleton.revision, status.skeleton.layout};
                result.revision        = status.poseRevision;
                result.retainedVersion = status.retainedVersion;
                result.diagnostic      = status.diagnostic.message;
                const auto *instance   = Find(actor);
                if (instance && instance->bound && (instance->state == AnimationSourceState::Ready || instance->retained)) {
                    result.state  = aurora::SkeletalPoseState::Ready;
                    result.models = instance->bound->models;
                } else {
                    result.state =
                        status.state == AnimationSourceState::Pending ? aurora::SkeletalPoseState::Pending : aurora::SkeletalPoseState::Error;
                }
                return result;
            }

        private:
            static AnimationStatus AdvanceBound(BoundAnimation &bound, float rate, bool loop, float delta, bool simulate)
            {
                if (bound.graph) {
                    const auto status = bound.graph->SetRate(rate);
                    return status && simulate ? bound.graph->Tick(delta) : status;
                }
                bound.player.SetLoop(loop);
                const auto status = bound.player.SetPlayRate(rate);
                return status ? bound.player.Tick(simulate ? delta : 0.f) : status;
            }

            static bool PollGraph(Instance &instance, AnimationSourceResult<Skeleton> rig, float delta, bool simulate)
            {
                const auto asset = instance.graphRequest;
                if (rig.state == AnimationSourceState::Error) {
                    Fail(instance, rig.status);
                    return false;
                }
                if (!asset || asset->GetStatus() == AssetBase::Status::FAILED) {
                    Fail(instance, {AnimationError::InvalidAsset, "Graph product or one of its dependencies failed to load"});
                    return false;
                }
                if (rig.state != AnimationSourceState::Ready || !asset->IsLoaded()) {
                    return false;
                }
                const auto &definition = asset->Data().graph.definition;
                if (!rig.resource || rig.version.asset != instance.config.skeleton || !rig.version.generation || !rig.version.revision ||
                    rig.version.layout != rig.resource->GetLayoutSignature() || definition.skeleton != rig.version.asset ||
                    asset->GetUuid() != instance.config.graph || !asset->Data().IsLoadValid()) {
                    Fail(instance, {AnimationError::InvalidAsset, "Graph and Skeleton versions are incompatible"});
                    return false;
                }
                if (instance.graphRequests.empty()) {
                    auto *source = Interface<IAnimationClipSource>::Get()->GetApi();
                    if (!source) {
                        Fail(instance, {AnimationError::MissingProvider, "Animation Clip provider is unavailable"});
                        return false;
                    }
                    std::unordered_set<Uuid> requested;
                    for (const auto &node : definition.nodes) {
                        if (node.kind == GraphNodeKind::Clip && requested.insert(node.clip).second) {
                            instance.graphRequests.emplace_back(node.clip, source->Request(node.clip));
                        }
                    }
                }
                std::vector<AnimationSourceResult<AnimationClip>> results;
                results.reserve(instance.graphRequests.size());
                bool pending = false;
                for (auto &[id, request] : instance.graphRequests) {
                    auto clip = request.Poll();
                    if (clip.state == AnimationSourceState::Error) {
                        Fail(instance, clip.status);
                        return false;
                    }
                    if (clip.state != AnimationSourceState::Ready) {
                        pending = true;
                        continue;
                    }
                    if (!clip.resource || clip.version.asset != id || !clip.version.generation || !clip.version.revision ||
                        (clip.skeletonHint && clip.skeletonHint != rig.version.asset)) {
                        Fail(instance, {AnimationError::InvalidAsset, "Graph Clip provider returned an incompatible version"});
                        return false;
                    }
                    const auto status = clip.resource->Validate(rig.resource.Get());
                    if (!status) {
                        Fail(instance, status);
                        return false;
                    }
                    results.push_back(std::move(clip));
                }
                if (pending) {
                    return false;
                }
                auto next        = std::make_unique<BoundAnimation>();
                next->graphAsset = asset;
                next->graphClips = std::move(results);
                next->rig        = std::move(rig);
                GraphInstance::Clips clips;
                for (const auto &clip : next->graphClips) {
                    clips.emplace(clip.version.asset, clip.resource);
                }
                AnimationStatus status;
                next->graph = GraphInstance::Create(asset->Data().graph, next->rig.resource, clips, status);
                if (!next->graph) {
                    Fail(instance, status);
                    return false;
                }
                std::unordered_set<Uuid> overrides;
                for (const auto &parameter : instance.config.parameters) {
                    if (!overrides.insert(parameter.parameter).second) {
                        Fail(instance, {AnimationError::InvalidAsset, "Graph initial parameter override is duplicated"});
                        return false;
                    }
                    status = next->graph->SetParameter(parameter.parameter, parameter.isBoolean ? AnimationParam(parameter.booleanValue)
                                                                                                : AnimationParam(parameter.floatValue));
                    if (!status) {
                        Fail(instance, status);
                        return false;
                    }
                }
                if (instance.bound && instance.bound->graph) {
                    const auto &old        = *instance.bound;
                    const auto &parameters = old.graphAsset->Data().graph.definition.parameters;
                    const auto &values     = old.graph->GetParameters();
                    for (size_t i = 0; i < parameters.size(); ++i) {
                        const auto found = std::find_if(definition.parameters.begin(), definition.parameters.end(),
                                                        [&](const auto &entry) { return entry.id == parameters[i].id; });
                        if (found == definition.parameters.end()) {
                            continue;
                        }
                        status = next->graph->SetParameter(parameters[i].id, values[i]);
                        if (!status) {
                            Fail(instance, status);
                            return false;
                        }
                    }
                }
                next->graph->SetPlaying(instance.bound ? instance.bound->graph->IsPlaying() : instance.config.autoplay);
                status = AdvanceBound(*next, instance.rate, false, delta, simulate);
                if (status) {
                    status = EvaluateBound(*next, instance.config.rootPolicy);
                }
                if (!status) {
                    Fail(instance, status);
                    return false;
                }
                instance.bound      = std::move(next);
                instance.pending    = false;
                instance.retained   = false;
                instance.rigRequest = {};
                instance.graphRequest.reset();
                instance.graphRequests.clear();
                instance.diagnostic = {};
                instance.state      = AnimationSourceState::Ready;
                ++instance.poseRevision;
                return true;
            }

            void Detach()
            {
                if (world) {
                    const auto found = registry->systems.find(world);
                    if (found != registry->systems.end() && found->second == this) {
                        registry->systems.erase(found);
                    }
                }
                world = nullptr;
                instances.clear();
            }
            static void Fail(Instance &instance, AnimationStatus status)
            {
                instance.pending     = false;
                instance.rigRequest  = {};
                instance.clipRequest = {};
                instance.graphRequest.reset();
                instance.graphRequests.clear();
                instance.state      = AnimationSourceState::Error;
                instance.diagnostic = std::move(status);
                instance.retained   = instance.bound != nullptr;
            }
            static void Request(Instance &instance)
            {
                instance.rigRequest  = {};
                instance.clipRequest = {};
                instance.graphRequest.reset();
                instance.graphRequests.clear();
                instance.retained   = instance.bound != nullptr;
                instance.pending    = false;
                instance.state      = AnimationSourceState::Pending;
                instance.diagnostic = {};
                if (instance.config.mode != AnimationMode::Clip && instance.config.mode != AnimationMode::Graph) {
                    Fail(instance, {AnimationError::Unsupported, "Unknown animation mode"});
                    return;
                }
                auto *rigs  = Interface<IAnimationSkeletonSource>::Get()->GetApi();
                auto *clips = Interface<IAnimationClipSource>::Get()->GetApi();
                if (!rigs || !clips) {
                    Fail(instance, {AnimationError::MissingProvider, "Animation Clip/Skeleton provider is unavailable"});
                    return;
                }
                instance.rigRequest = rigs->Request(instance.config.skeleton);
                if (instance.config.mode == AnimationMode::Clip) {
                    instance.clipRequest = clips->Request(instance.config.clip);
                } else {
                    instance.graphRequest = AssetManager::Get()->LoadAsset<GraphAsset>(instance.config.graph, AssetLoadPolicy::ProductsOnly);
                }
                instance.pending = true;
            }
            static bool Poll(Instance &instance, float delta, bool running)
            {
                if (!instance.pending) {
                    return false;
                }
                auto rig = instance.rigRequest.Poll();
                if (instance.config.mode == AnimationMode::Graph) {
                    return PollGraph(instance, std::move(rig), delta, running);
                }
                auto clip = instance.clipRequest.Poll();
                if (rig.state == AnimationSourceState::Error) {
                    Fail(instance, rig.status);
                    return false;
                }
                if (clip.state == AnimationSourceState::Error) {
                    Fail(instance, clip.status);
                    return false;
                }
                if (rig.state != AnimationSourceState::Ready || clip.state != AnimationSourceState::Ready) {
                    return false;
                }
                if (!rig.resource || !clip.resource || rig.version.asset != instance.config.skeleton || clip.version.asset != instance.config.clip ||
                    !rig.version.generation || !clip.version.generation || !rig.version.revision || !clip.version.revision ||
                    rig.version.layout != rig.resource->GetLayoutSignature() || (clip.skeletonHint && clip.skeletonHint != rig.version.asset)) {
                    Fail(instance, {AnimationError::InvalidAsset, "Animation provider returned an incompatible asset version"});
                    return false;
                }
                auto status = clip.resource->Validate(rig.resource.Get());
                if (!status) {
                    Fail(instance, status);
                    return false;
                }
                auto next           = std::make_unique<BoundAnimation>();
                next->rig           = std::move(rig);
                next->clip          = std::move(clip);
                next->pose.skeleton = next->rig.resource.Get();
                next->pose.ResetRefPose();
                status = next->player.SetClip(next->clip.resource);
                next->player.SetLoop(instance.config.loop);
                if (status) {
                    status = next->player.SetPlayRate(instance.rate);
                }
                if (status && instance.bound) {
                    status = next->player.Seek(instance.bound->player.GetCurrentTime());
                }
                if (status) {
                    status = next->player.SetPlaying(instance.bound ? instance.bound->player.IsPlaying() : instance.config.autoplay);
                }
                if (!status) {
                    Fail(instance, status);
                    return false;
                }
                if (status) {
                    status = next->player.Tick(delta);
                }
                if (!status) {
                    Fail(instance, status);
                    return false;
                }
                // Validate and sample the entire candidate before replacing the live resource tuple.
                status = EvaluateBound(*next, instance.config.rootPolicy);
                if (!status) {
                    Fail(instance, status);
                    return false;
                }
                instance.bound       = std::move(next);
                instance.pending     = false;
                instance.retained    = false;
                instance.rigRequest  = {};
                instance.clipRequest = {};
                instance.graphRequest.reset();
                instance.graphRequests.clear();
                instance.diagnostic = {};
                instance.state      = AnimationSourceState::Ready;
                ++instance.poseRevision;
                return true;
            }
            static AnimationStatus EvaluateBound(BoundAnimation &bound, AnimationRootPolicy rootPolicy)
            {
                if (bound.graph) {
                    bound.graph->Evaluate(bound.pose);
                } else {
                    const auto status = bound.clip.resource->SamplePose(
                        bound.pose,
                        {Anim::ConvertFromFrameRate(bound.player.GetCurrentTime(), bound.clip.resource->GetPlayRate()), AnimInterpolation::LINEAR});
                    if (!status) {
                        return status;
                    }
                }
                if (rootPolicy == AnimationRootPolicy::LockToReference) {
                    for (const auto *root : bound.rig.resource->GetRoots()) {
                        bound.pose.transforms[root->index] = bound.rig.resource->GetRefPos()->transforms[root->index];
                    }
                }
                return bound.pose.ToSkinRenderData(bound.models, Transform{});
            }
            static AnimationStatus Evaluate(Instance &instance)
            {
                const auto status = EvaluateBound(*instance.bound, instance.config.rootPolicy);
                if (status) {
                    ++instance.poseRevision;
                }
                return status;
            }
            std::shared_ptr<Registry>                           registry;
            World                                              *world = nullptr;
            Uuid                                                epoch;
            bool                                                running      = false;
            uint64_t                                            clipSerial   = 0;
            uint64_t                                            rigSerial    = 0;
            uint64_t                                            graphSerial  = 0;
            const IAnimationClipSource                         *clipProvider = nullptr;
            const IAnimationSkeletonSource                     *rigProvider  = nullptr;
            std::unordered_map<Uuid, std::unique_ptr<Instance>> instances;
        };
        class Host final : public AnimationWorldHost {
        public:
            explicit Host(std::shared_ptr<const void> code) : registry(std::make_shared<Registry>())
            {
                registry->code = std::move(code);
            }
            ~Host() override
            {
                Drain();
            }
            std::unique_ptr<IWorldSubSystem> CreateSystem() override
            {
                return registry->active ? std::make_unique<System>(registry) : nullptr;
            }
            void Drain() override
            {
                registry->active = false;
                for (const auto &[world, system] : registry->systems) {
                    system->Drain();
                }
            }
            AnimationInstanceStatus Find(const World *world, const Uuid &actor) const override
            {
                const auto *system = Get(world);
                if (system) {
                    return system->Status(actor);
                }
                AnimationInstanceStatus result;
                result.state      = AnimationSourceState::Error;
                result.diagnostic = {AnimationError::MissingProvider, "World has no Animation subsystem"};
                return result;
            }
            AnimationStatus Control(const AnimationInstanceHandle &handle, AnimationControlOp op, float value) override
            {
                auto *system = Get(handle.world);
                return system ? system->Control(handle, op, value) : Stale();
            }
            AnimationStatus SetParameter(const AnimationInstanceHandle &handle, const Uuid &id, const AnimationParam &value) override
            {
                auto *system = Get(handle.world);
                return system ? system->SetParameter(handle, id, value) : Stale();
            }
            aurora::SkeletalPoseResult Describe(const void *world, const Uuid &actor) const override
            {
                const auto *system = Get(world);
                if (system) {
                    return system->Describe(actor);
                }
                aurora::SkeletalPoseResult result;
                result.state      = aurora::SkeletalPoseState::Error;
                result.diagnostic = "World has no Animation subsystem";
                return result;
            }
            aurora::SkeletalPoseResult Query(const aurora::SkeletalPoseIdentity &identity, const aurora::SkeletalRigVersion &rig) const override
            {
                auto result = Describe(identity.world, identity.actor);
                if (result.instance.worldEpoch != identity.worldEpoch || result.instance.generation != identity.generation ||
                    result.skeleton != rig) {
                    result.state      = aurora::SkeletalPoseState::Error;
                    result.diagnostic = "Animation instance or Skeleton version is stale";
                    result.models.clear();
                }
                return result;
            }

        private:
            System *Get(const void *world) const
            {
                const auto found = registry->systems.find(world);
                return registry->active && found != registry->systems.end() ? found->second : nullptr;
            }
            std::shared_ptr<Registry> registry;
        };
    } // namespace
    std::unique_ptr<AnimationWorldHost> CreateAnimationWorldHost(std::shared_ptr<const void> moduleLease)
    {
        return moduleLease ? std::make_unique<Host>(std::move(moduleLease)) : nullptr;
    }
} // namespace sky::animation
