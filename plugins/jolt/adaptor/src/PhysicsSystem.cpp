//
// Created on 2026/09/23.
//

#include <jolt/PhysicsSystem.h>
#include <jolt/components/PhysicsBodyComponent.h>

#include <physics/PhysicsBackendRegistry.h>

#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <algorithm>
#include <cmath>
#include <core/logger/Logger.h>

namespace sky::phy {

    PhysicsSystem::~PhysicsSystem()
    {
        closing      = true;
        auto closing = std::move(stepListeners);
        if (world) {
            for (const auto &listener : closing) {
                listener->OnPhysicsWorldClosing(*world);
            }
        }
    }

    bool PhysicsSystem::Init(const PhysicsWorldDesc &desc)
    {
        if (world || stepping || closing) {
            status = {PhysicsError::InvalidState, "Physics system is already initialized"};
            return false;
        }
        world  = std::unique_ptr<IPhysicsWorld>(PhysicsBackendRegistry::Get().CreateWorld(desc));
        status = world ? PhysicsStatus{} : PhysicsStatus{PhysicsError::BackendFailure, "Physics world creation failed"};
        return world != nullptr;
    }

    void PhysicsSystem::OnAttachToWorld(World &world)
    {
        attachedWorld = &world;
    }

    void PhysicsSystem::OnDetachFromWorld(World &world)
    {
        attachedWorld = nullptr;
    }

    void PhysicsSystem::StartSimulation()
    {
        SetSimulating(true);
    }

    void PhysicsSystem::StopSimulation()
    {
        SetSimulating(false);
    }

    void PhysicsSystem::SetSimulating(bool value)
    {
        simulating = value;
    }

    bool PhysicsSystem::IsSimulating() const
    {
        return simulating;
    }

    void PhysicsSystem::SingleStep()
    {
        RunSteps(1, 1.f);
    }

    void PhysicsSystem::Tick(float time)
    {
        if (stepping || closing) {
            status = {PhysicsError::InvalidState, "Physics stepping is reentrant or closing"};
            return;
        }
        pendingEvents.clear();
        if (world == nullptr || !simulating || !std::isfinite(time) || time < 0) {
            return;
        }
        const uint32_t steps = stepper.Advance(time);
        RunSteps(steps, stepper.GetInterpolationAlpha());
    }

    void PhysicsSystem::RunSteps(uint32_t count, float alpha)
    {
        if (stepping || closing) {
            status = {PhysicsError::InvalidState, "Recursive physics stepping is unsupported"};
            return;
        }
        pendingEvents.clear();
        if (!world) {
            status = {PhysicsError::InvalidState, "Physics world is unavailable"};
            return;
        }
        std::vector<PhysicsEvent> completed;
        stepping = true;
        for (uint32_t i = 0; i < count; ++i) {
            const float delta    = stepper.GetConfig().fixedDelta;
            const auto  snapshot = stepListeners;
            status               = {};
            for (const auto &listener : snapshot) {
                if (std::find(stepListeners.begin(), stepListeners.end(), listener) != stepListeners.end()) {
                    status = listener->BeforePhysicsStep(*world, delta);
                    if (!status) {
                        break;
                    }
                }
            }
            if (status) {
                status = world->Step(delta);
            }
            if (!status) {
                simulating = false;
                LOG_E("Jolt", "%s", status.message.c_str());
                break;
            }
            world->DrainEvents(completed);
            for (const auto &listener : snapshot) {
                if (std::find(stepListeners.begin(), stepListeners.end(), listener) != stepListeners.end()) {
                    const auto published = listener->AfterPhysicsStep(*world, delta);
                    if (!published && status) {
                        status = published;
                    }
                }
            }
            if (!status) {
                simulating = false;
                LOG_E("Jolt", "%s", status.message.c_str());
                break;
            }
        }
        if (status) {
            world->SetInterpolationAlpha(alpha);
            presentationAlpha = alpha;
        }
        stepping = false;
        pendingEvents.insert(pendingEvents.end(), completed.begin(), completed.end());
        if (!completed.empty()) {
            const auto snapshot = listeners;
            for (auto *listener : snapshot) {
                if (std::find(listeners.begin(), listeners.end(), listener) != listeners.end())
                    listener->OnPhysicsEvents(completed);
            }
        }
    }

    PhysicsStatus PhysicsSystem::AddStepListener(std::shared_ptr<IPhysicsStepListener> listener)
    {
        if (closing) {
            return {PhysicsError::InvalidState, "Physics world is closing"};
        }
        if (!listener) {
            return {PhysicsError::InvalidArgument, "Physics step listener is null"};
        }
        if (std::find(stepListeners.begin(), stepListeners.end(), listener) != stepListeners.end()) {
            return {PhysicsError::InvalidArgument, "Physics step listener is already registered"};
        }
        stepListeners.push_back(std::move(listener));
        return {};
    }

    void PhysicsSystem::RemoveStepListener(const IPhysicsStepListener *listener)
    {
        std::erase_if(stepListeners, [listener](const auto &entry) { return entry.get() == listener; });
    }

    PhysicsStatus PhysicsSystem::SetOptions(const PhysicsOptions &options)
    {
        if (world != nullptr) {
            status = world->SetOptions(options);
        } else {
            status = {PhysicsError::InvalidState, "Physics world is unavailable"};
        }
        return status;
    }

    PhysicsOptions PhysicsSystem::GetOptions() const
    {
        return world != nullptr ? world->GetOptions() : PhysicsOptions{};
    }

    void PhysicsSystem::AddEventListener(IPhysicsEventListener *listener)
    {
        if (listener != nullptr && std::find(listeners.begin(), listeners.end(), listener) == listeners.end()) {
            listeners.push_back(listener);
        }
    }

    void PhysicsSystem::RemoveEventListener(IPhysicsEventListener *listener)
    {
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }

    void PhysicsSystem::DrainEvents(std::vector<PhysicsEvent> &out)
    {
        out.insert(out.end(), pendingEvents.begin(), pendingEvents.end());
        pendingEvents.clear();
    }

    PhysicsWorldStats PhysicsSystem::GetStats() const
    {
        return world != nullptr ? world->GetStats() : PhysicsWorldStats{};
    }

    void PhysicsSystem::GatherCollisionMeshes(std::vector<CollisionMeshInstance> &out) const
    {
        if (attachedWorld == nullptr) {
            return;
        }
        for (const auto &actor : attachedWorld->GetActors()) {
            auto *body = actor->GetComponent<PhysicsBodyComponent>();
            if (body == nullptr || !IsValid(body->GetObjectId())) {
                continue;
            }
            const ShapeDesc &shape = body->GetResolvedShape();
            if (shape.type != ShapeType::TriangleMesh || !shape.meshData) {
                continue;
            }
            CollisionMeshInstance instance;
            instance.mesh = shape.meshData;
            if (auto *transform = actor->GetComponent<TransformComponent>()) {
                instance.transform = transform->GetWorldTransform();
            }
            out.push_back(instance);
        }
    }

    bool PhysicsSystem::BeginAttachmentGroup(std::span<Actor *const> actors, std::string &error)
    {
        if (!world) {
            error = "Physics world is unavailable";
            return false;
        }
        uint32_t count = 0;
        for (auto *actor : actors)
            count += actor->GetComponent<PhysicsBodyComponent>() ? 1 : 0;
        status = world->BeginActivation(count);
        error  = status.message;
        return static_cast<bool>(status);
    }
    void PhysicsSystem::EndAttachmentGroup(bool commit)
    {
        if (world)
            world->EndActivation(commit);
    }
    void PhysicsSystem::BeforeDetachGroup(std::span<Actor *const> actors)
    {
        for (auto *actor : actors)
            if (auto *body = actor->GetComponent<PhysicsBodyComponent>(); body && IsValid(body->GetObjectId()))
                world->DestroyObject(body->GetObjectId());
    }
    bool PhysicsSystem::CanDetachGroup(std::span<Actor *const> actors) const
    {
        if (!world)
            return false;
        std::vector<PhysicsObjectId> bodies;
        for (auto *actor : actors)
            if (auto *body = actor->GetComponent<PhysicsBodyComponent>(); body && IsValid(body->GetObjectId()))
                bodies.push_back(body->GetObjectId());
        return world->CanRetire(bodies);
    }

    PhysicsSystem *AttachPhysicsSystem(World &world, const PhysicsWorldDesc &desc)
    {
        auto system = std::make_unique<PhysicsSystem>();
        if (!system->Init(desc)) {
            return nullptr;
        }
        auto *raw = system.get();
        world.AddSubSystem(Name(PhysicsSystem::NAME.data()), system.release());
        return raw;
    }

} // namespace sky::phy
