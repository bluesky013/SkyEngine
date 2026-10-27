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

    bool PhysicsSystem::Init(const PhysicsWorldDesc &desc)
    {
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
        pendingEvents.clear();
        if (world == nullptr || !simulating || !std::isfinite(time) || time < 0) {
            return;
        }
        const uint32_t steps = stepper.Advance(time);
        RunSteps(steps, stepper.GetInterpolationAlpha());
    }

    void PhysicsSystem::RunSteps(uint32_t count, float alpha)
    {
        pendingEvents.clear();
        if (!world) {
            status = {PhysicsError::InvalidState, "Physics world is unavailable"};
            return;
        }
        std::vector<PhysicsEvent> completed;
        for (uint32_t i = 0; i < count; ++i) {
            status = world->Step(stepper.GetConfig().fixedDelta);
            if (!status) {
                simulating = false;
                LOG_E("Jolt", "%s", status.message.c_str());
                break;
            }
            world->DrainEvents(completed);
        }
        if (status) {
            world->SetInterpolationAlpha(alpha);
        }
        pendingEvents.insert(pendingEvents.end(), completed.begin(), completed.end());
        if (!completed.empty()) {
            const auto snapshot = listeners;
            for (auto *listener : snapshot) {
                if (std::find(listeners.begin(), listeners.end(), listener) != listeners.end())
                    listener->OnPhysicsEvents(completed);
            }
        }
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
            const ShapeDesc &shape = body->GetShape();
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

    PhysicsSystem *AttachPhysicsSystem(World &world, const PhysicsWorldDesc &desc, std::shared_ptr<const void> lease)
    {
        auto system = std::make_unique<PhysicsSystem>();
        if (!system->Init(desc)) {
            return nullptr;
        }
        auto *raw = system.get();
        world.AddSubSystem(Name(PhysicsSystem::NAME.data()), system.release(), std::move(lease));
        return raw;
    }

} // namespace sky::phy
