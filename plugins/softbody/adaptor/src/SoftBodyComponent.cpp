#include <softbody/world/SoftBodyComponent.h>

#include "SoftBodyWorldCoordinator.h"

#include <animation/IAnimationPoseContributor.h>
#include <core/logger/Logger.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>
#include <physics/IPhysicsSystem.h>
#include <physics/softbody/ISoftBodyCollisionQuery.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/solver/CpuSoftBodySolver.h>
#include <softbody/world/SoftBodyPose.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace sky::softbody {
    namespace {
        phy::PhysicsStatus Unavailable(const char *message)
        {
            return {phy::PhysicsError::InvalidState, message};
        }
        Transform ActorRoot(Actor &actor)
        {
            const auto *transform = actor.GetComponent<TransformComponent>();
            return transform ? transform->GetWorldTransform() : Transform{};
        }
        phy::PhysicsStatus ValidateSnapshot(const phy::SoftBodySnapshot &snapshot, const Uuid &expected, size_t count)
        {
            const auto finite = [](const Vector3 &p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
            if (snapshot.id != expected || snapshot.worldPositions.size() != count || snapshot.worldVelocities.size() != count ||
                !std::all_of(snapshot.worldPositions.begin(), snapshot.worldPositions.end(), finite) ||
                !std::all_of(snapshot.worldVelocities.begin(), snapshot.worldVelocities.end(), finite))
                return {phy::PhysicsError::BackendFailure, "Soft body returned an incomplete or nonfinite snapshot"};
            return {};
        }
    } // namespace

    struct SoftBodyComponent::Resources {
        Uuid                                             requested;
        uint64_t                                         productSerial = 0;
        std::shared_ptr<AssetLoadRequest>                request;
        std::shared_ptr<Asset<SoftBodyAsset>>            rig;
        std::array<std::shared_ptr<AssetLoadRequest>, 3> dependencies;
        std::array<AssetPtr, 3>                          leases;
        ComponentPreparation                             result = ComponentPreparation::Preparing;
        std::string                                      error;

        void Start(const Uuid &id)
        {
            requested     = id;
            productSerial = AssetManager::Get()->GetProductChangeSerial();
            request       = AssetManager::Get()->LoadAssetAsync(id, AssetLoadPolicy::ProductsOnly);
            if (!request)
                Fail("Soft body product request could not be started");
        }
        ComponentPreparation Fail(const char *message)
        {
            error  = message;
            result = ComponentPreparation::Failed;
            return result;
        }
        ComponentPreparation Poll()
        {
            if (result != ComponentPreparation::Preparing)
                return result;
            if (!rig) {
                if (!request || !request->IsResolved())
                    return result;
                const auto value = request->GetAsset();
                if (!value || value->GetType() != Name(SOFT_BODY_ASSET_TYPE.data()) || value->GetStatus() == AssetBase::Status::FAILED)
                    return Fail("Soft body product is missing or has the wrong type");
                if (!value->IsLoaded())
                    return result;
                rig = std::static_pointer_cast<Asset<SoftBodyAsset>>(value);
                if (const auto valid = ValidateSoftBodyAsset(rig->Data()); !valid)
                    return Fail(valid.message.c_str());
                const auto               &data = rig->Data();
                const std::array<Uuid, 3> ids{data.mesh, data.skin, data.skeleton};
                for (size_t i = 0; i < ids.size(); ++i) {
                    dependencies[i] = AssetManager::Get()->LoadAssetAsync(ids[i], AssetLoadPolicy::ProductsOnly);
                    if (!dependencies[i])
                        return Fail("Soft body dependency request could not be started");
                }
            }
            constexpr std::array<const char *, 3> types{"AuroraMesh", "AuroraSkin", "AuroraSkeleton"};
            for (size_t i = 0; i < dependencies.size(); ++i) {
                if (!dependencies[i]->IsResolved())
                    return result;
                const auto value = dependencies[i]->GetAsset();
                if (!value || value->GetType() != Name(types[i]) || value->GetStatus() == AssetBase::Status::FAILED)
                    return Fail("Soft body dependency is missing or has the wrong type");
                if (!value->IsLoaded())
                    return result;
                leases[i] = value;
            }
            result = ComponentPreparation::Ready;
            return result;
        }
    };

    struct SoftBodyComponent::State final : SoftBodyWorldMember, IAnimationPoseContributor {
        Actor                                   *actor = nullptr;
        World                                   *world = nullptr;
        Uuid                                     epoch;
        phy::IPhysicsSystem                     *host    = nullptr;
        phy::IPhysicsWorld                      *physics = nullptr;
        std::weak_ptr<SoftBodyWorldRegistration> coordinator;
        std::unique_ptr<CpuSoftBodySolver>       solver;
        Uuid                                     id;
        std::shared_ptr<Asset<SoftBodyAsset>>    rig;
        std::array<AssetPtr, 3>                  leases;
        phy::SoftBodySnapshot                    previous, current;
        Transform                                presentationRoot;
        std::vector<Vector3>                     presentation;
        uint64_t                                 presentationStep  = 0;
        float                                    presentationAlpha = 1.f;
        uint64_t                                 revision          = 1;
        bool                                     needsRootTarget   = false;
        AnimationPoseContributionToken           token;
        IAnimationPoseContributionRegistry      *registry = nullptr;
        phy::PhysicsStatus                       diagnostic;

        ~State() override
        {
            Stop();
        }

        bool Live() const
        {
            return actor && world && host && physics;
        }
        void Unbind()
        {
            auto *api = Interface<IAnimationPoseContributionRegistry>::Get()->GetApi();
            if (token.id && api && api == registry) {
                const auto result = api->UnregisterPoseContributor(token);
                if (!result && result.error != AnimationError::StaleGeneration)
                    LOG_E("SoftBody", "%s", result.message.c_str());
            }
            token    = {};
            registry = nullptr;
        }
        void Stop()
        {
            Unbind();
            if (const auto registration = coordinator.lock())
                registration->Remove(this);
            coordinator.reset();
            solver.reset();
            actor   = nullptr;
            world   = nullptr;
            host    = nullptr;
            physics = nullptr;
            id      = {};
            rig.reset();
            leases = {};
            presentation.clear();
            previous = {};
            current  = {};
        }
        phy::PhysicsStatus Disable()
        {
            solver.reset();
            Unbind();
            id = {};
            rig.reset();
            leases   = {};
            previous = {};
            current  = {};
            presentation.clear();
            diagnostic = {};
            return {};
        }
        void Close(phy::IPhysicsWorld &closing) override
        {
            if (physics != &closing)
                return;
            Unbind();
            solver.reset();
            // Closing revokes the borrowed collision world before any later UI or pose callback.
            actor   = nullptr;
            world   = nullptr;
            host    = nullptr;
            physics = nullptr;
            coordinator.reset();
            id         = {};
            diagnostic = Unavailable("Soft body physics world has closed");
        }
        Uuid GetOrderKey() const override
        {
            return actor ? actor->GetUuid() : Uuid{};
        }
        CpuSoftBodySolver *GetSolver() const override
        {
            return Live() ? solver.get() : nullptr;
        }
        phy::PhysicsStatus ReadDebugBody(phy::SoftBodyDebugBody &out) const override
        {
            if (!Live() || !id || !rig || !solver || world->GetRuntimeId() != epoch)
                return Unavailable("Soft body has no live debug publication");
            const auto &body = rig->Data().body;
            if (const auto status = ValidateSnapshot(current, id, body.mesh.vertices.size()); !status)
                return status;
            if (presentation.size() != current.worldPositions.size() || !std::all_of(presentation.begin(), presentation.end(), [](const auto &p) {
                    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
                }))
                return Unavailable("Soft body presentation is incomplete or nonfinite");
            phy::SoftBodyDebugBody result;
            result.id                      = id;
            result.actor                   = actor->GetUuid();
            result.step                    = current.step;
            result.revision                = current.revision;
            result.presentationStep        = presentationStep;
            result.presentationRevision    = revision;
            result.interpolationAlpha      = presentationAlpha;
            result.vertexRadius            = body.vertexRadius;
            result.worldPositions          = presentation;
            result.committedWorldPositions = current.worldPositions;
            result.faces                   = body.mesh.faces;
            result.edges.reserve(body.mesh.edges.size());
            for (const auto &edge : body.mesh.edges)
                result.edges.push_back(edge.vertices);
            out = std::move(result);
            return {};
        }
        phy::PhysicsStatus UpdateRoot(phy::IPhysicsWorld &stepping) override
        {
            if (!Live() || physics != &stepping || world->GetRuntimeId() != epoch)
                return diagnostic = Unavailable("Soft body world binding is stale");
            if (!id || !needsRootTarget)
                return {};
            return diagnostic = solver->SetRootTarget(ActorRoot(*actor));
        }
        phy::PhysicsStatus ValidatePublication(const phy::SoftBodySnapshot &next) const override
        {
            if (!Live() || !rig || world->GetRuntimeId() != epoch)
                return Unavailable("Soft body world binding is stale");
            return ValidateSnapshot(next, id, rig->Data().body.mesh.vertices.size());
        }
        void Publish(phy::SoftBodySnapshot next) override
        {
            previous = std::move(current);
            current  = std::move(next);
        }
        void Report(const phy::PhysicsStatus &result) override
        {
            diagnostic = result;
        }
        void Present()
        {
            if (!Live() || !id)
                return;
            if (revision == std::numeric_limits<uint64_t>::max()) {
                diagnostic = Unavailable("Soft body presentation revision exhausted");
                return;
            }
            const float alpha = std::clamp(host->GetInterpolationAlpha(), 0.f, 1.f);
            presentationAlpha = alpha;
            presentationStep  = current.step;
            presentationRoot  = ActorRoot(*actor);
            presentation      = current.worldPositions;
            if (previous.worldPositions.size() == presentation.size())
                for (size_t i = 0; i < presentation.size(); ++i)
                    presentation[i] = previous.worldPositions[i] * (1.f - alpha) + presentation[i] * alpha;
            ++revision;
        }
        AnimationPoseContribution Evaluate(const AnimationPoseBase &base) const override
        {
            AnimationPoseContribution result;
            result.binding      = base.binding;
            result.baseRevision = base.revision;
            result.revision     = revision;
            if (!Live() || !rig || !token.id || base.binding != token.binding) {
                result.status = {AnimationError::StaleGeneration, "Soft body contribution binding is stale"};
                return result;
            }
            const auto status = BuildSoftBodyPose(rig->Data(), presentation, presentationRoot, base.models, result.models, result.modelNormals);
            if (!status)
                result.status = {AnimationError::InvalidAsset, status.message};
            return result;
        }
        phy::PhysicsStatus Bind(const AnimationPoseBinding &binding, const std::shared_ptr<State> &self)
        {
            auto *api = Interface<IAnimationPoseContributionRegistry>::Get()->GetApi();
            if (!api)
                return Unavailable("Animation contribution registry is unavailable");
            if (token.id && registry == api && token.binding == binding)
                return {};
            const auto old         = token;
            auto      *oldRegistry = registry;
            Unbind();
            AnimationPoseContributionToken next;
            const auto                     result = api->RegisterPoseContributor(binding, self, next);
            if (!result) {
                if (old.id && oldRegistry == api) {
                    const auto restored = api->RegisterPoseContributor(old.binding, self, token);
                    if (restored)
                        registry = api;
                    else
                        LOG_E("SoftBody", "Contribution rollback failed: %s", restored.message.c_str());
                }
                return {phy::PhysicsError::InvalidState, result.message};
            }
            token    = std::move(next);
            registry = api;
            return {};
        }
        phy::PhysicsStatus Install(const Resources &resources, const AnimationPoseBinding &binding, const std::shared_ptr<State> &self)
        {
            if (!Live())
                return Unavailable("Soft body world binding is stale");
            if (rig == resources.rig)
                return Bind(binding, self);
            auto desc              = resources.rig->Data().body;
            desc.transform         = ActorRoot(*actor);
            const auto candidateId = Uuid::Create();
            auto       candidate   = std::make_unique<CpuSoftBodySolver>();
            if (const auto result = candidate->Initialize(desc, candidateId); !result)
                return result;
            auto snapshot = candidate->GetSnapshot();
            if (const auto result = ValidateSnapshot(snapshot, candidateId, desc.mesh.vertices.size()); !result)
                return result;
            if (const auto result = Bind(binding, self); !result)
                return result;
            solver          = std::move(candidate);
            id              = candidateId;
            rig             = resources.rig;
            leases          = resources.leases;
            current         = std::move(snapshot);
            previous        = current;
            needsRootTarget = !desc.rootFollow.empty() || std::any_of(desc.mesh.vertices.begin(), desc.mesh.vertices.end(), [](const auto &v) {
                return v.mode == phy::SoftBodyAttachmentMode::RootKinematic;
            });
            Present();
            diagnostic = {};
            return {};
        }
    };

    SoftBodyComponent::SoftBodyComponent() = default;
    SoftBodyComponent::~SoftBodyComponent()
    {
        OnDetachFromWorld();
    }

    bool SoftBodyComponent::SetRig(const Uuid &rig)
    {
        if (data.rig == rig && resources && resources->productSerial == AssetManager::Get()->GetProductChangeSerial())
            return true;
        if (!rig && state) {
            status = state->Disable();
            if (!status)
                return false;
        }
        data.rig = rig;
        CancelPreparation();
        status = {};
        if (!rig) {
            return true;
        }
        resources = std::make_unique<Resources>();
        resources->Start(rig);
        return resources->result != ComponentPreparation::Failed;
    }
    void SoftBodyComponent::CollectAssetReferences(std::vector<ComponentAssetReference> &out) const
    {
        if (data.rig)
            out.push_back({data.rig, std::string(SOFT_BODY_ASSET_TYPE)});
    }
    ComponentPreparation SoftBodyComponent::PrepareForWorld(World &, std::string &error)
    {
        if (!data.rig) {
            error.clear();
            return ComponentPreparation::Ready;
        }
        if (!resources || resources->requested != data.rig) {
            resources = std::make_unique<Resources>();
            resources->Start(data.rig);
        }
        return PollWorldPreparation(error);
    }
    ComponentPreparation SoftBodyComponent::PollWorldPreparation(std::string &error)
    {
        if (!resources) {
            if (!data.rig) {
                error.clear();
                return ComponentPreparation::Ready;
            }
            error = "Soft body product preparation has not started";
            return ComponentPreparation::Failed;
        }
        const auto result = resources->Poll();
        error             = resources->error;
        if (result == ComponentPreparation::Failed)
            status = Unavailable(error.c_str());
        return result;
    }
    void SoftBodyComponent::CancelPreparation()
    {
        resources.reset();
    }

    bool SoftBodyComponent::TryAttachToWorld(std::string &error)
    {
        auto *world = actor ? actor->GetWorld() : nullptr;
        if (!world || !actor->GetComponent<TransformComponent>()) {
            error = "Soft body requires an actor world and transform";
            return false;
        }
        auto *system = static_cast<phy::IPhysicsSystem *>(world->GetSubSystem(Name(phy::PHYSICS_SYSTEM_NAME.data())));
        if (!system || !system->GetWorld() || !system->GetWorld()->GetSoftBodyCollisionQuery()) {
            error = "World does not provide soft body collision queries";
            return false;
        }
        auto *animation = Interface<IAnimationInstanceControl>::Get()->GetApi();
        if (!animation || !Interface<IAnimationPoseContributionRegistry>::Get()->GetApi() ||
            !world->GetSubSystem(Name(animation->GetWorldSystemName().data()))) {
            error = "World does not provide animation contributions";
            return false;
        }
        if (PrepareForWorld(*world, error) == ComponentPreparation::Failed)
            return false;
        auto next     = std::make_shared<State>();
        next->actor   = actor;
        next->world   = world;
        next->epoch   = world->GetRuntimeId();
        next->host    = system;
        next->physics = system->GetWorld();
        std::shared_ptr<SoftBodyWorldRegistration> registration;
        const auto                                 result = RegisterSoftBodyWorldMember(next->epoch, *system, next, registration);
        if (!result) {
            error = result.message;
            next->Stop();
            return false;
        }
        next->coordinator = registration;
        if (state)
            state->Stop();
        state = std::move(next);
        return true;
    }
    void SoftBodyComponent::OnDetachFromWorld()
    {
        CancelPreparation();
        if (state)
            state->Stop();
        state.reset();
    }
    void SoftBodyComponent::Tick(float)
    {
        if (!state || !state->Live())
            return;
        if (!resources || resources->requested != data.rig || resources->productSerial != AssetManager::Get()->GetProductChangeSerial()) {
            if (!data.rig)
                return;
            resources = std::make_unique<Resources>();
            resources->Start(data.rig);
        }
        if (resources->Poll() != ComponentPreparation::Ready) {
            if (resources->result == ComponentPreparation::Failed)
                status = Unavailable(resources->error.c_str());
            return;
        }
        auto *api = Interface<IAnimationInstanceControl>::Get()->GetApi();
        if (!api) {
            status = Unavailable("Animation control is unavailable");
            return;
        }
        const auto animation = api->Find(state->world, actor->GetUuid());
        if (animation.state != AnimationSourceState::Ready && !animation.retainedVersion) {
            status = Unavailable("Waiting for the actor reference pose");
            return;
        }
        const auto &rig      = resources->rig->Data();
        const auto &skeleton = resources->leases[2];
        if (animation.handle.world != state->world || animation.handle.worldEpoch != state->epoch || animation.handle.actor != actor->GetUuid() ||
            animation.skeleton.asset != rig.skeleton || animation.skeleton.layout != rig.skeletonLayout ||
            animation.skeleton.generation != skeleton->GetInstanceId() || animation.skeleton.revision != skeleton->GetRevision()) {
            status = Unavailable("Soft body and animation require the same skeleton product version");
            return;
        }
        status = state->Install(*resources, {animation.handle, animation.skeleton}, state);
    }
    void SoftBodyComponent::PostTick(float)
    {
        if (state)
            state->Present();
    }
    phy::PhysicsStatus SoftBodyComponent::GetLastStatus() const
    {
        return !status ? status : state ? state->diagnostic : Unavailable("Soft body is not attached");
    }
    phy::PhysicsStatus SoftBodyComponent::GetSnapshot(phy::SoftBodySnapshot &out) const
    {
        if (!state || !state->Live() || !state->id)
            return Unavailable("Soft body has no live snapshot");
        out = state->current;
        return {};
    }
    phy::PhysicsStatus SoftBodyComponent::Reset()
    {
        if (!state || !state->Live() || !state->id)
            return status = Unavailable("Soft body is not attached");
        if (const auto result = state->solver->Reset(ActorRoot(*actor)); !result)
            return status = result;
        const auto &snapshot = state->solver->GetSnapshot();
        if (const auto result = ValidateSnapshot(snapshot, state->id, state->rig->Data().body.mesh.vertices.size()); !result)
            return status = result;
        state->current    = snapshot;
        state->previous   = state->current;
        state->diagnostic = {};
        state->Present();
        return status = {};
    }
    void SoftBodyComponent::Reflect(SerializationContext *context)
    {
        context->Register<SoftBodyComponent>("SoftBodyComponent")
            .Member<&SoftBodyComponent::SetRig, &SoftBodyComponent::GetRig>("rig")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string(SOFT_BODY_ASSET_TYPE)));
        ComponentFactory::Get()->RegisterComponent<SoftBodyComponent>("SoftBody", {Name(phy::PHYSICS_SYSTEM_NAME.data()), Name("Animation")});
    }
} // namespace sky::softbody
