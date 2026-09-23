//
// Created on 2026/09/23.
//

#include <bullet/PhysicsSystem.h>
#include <bullet/components/PhysicsBodyComponent.h>

#include <physics/PhysicsBackendRegistry.h>

#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <algorithm>

namespace sky::phy {

    bool PhysicsSystem::Init(const PhysicsWorldDesc &desc)
    {
        world = std::unique_ptr<IPhysicsWorld>(PhysicsBackendRegistry::Get().CreateWorld(desc));
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
        if (world != nullptr) {
            world->Step(stepper.GetConfig().fixedDelta);
        }
    }

    void PhysicsSystem::Tick(float time)
    {
        if (world == nullptr || !simulating) {
            return;
        }
        const uint32_t steps = stepper.Advance(time);
        for (uint32_t i = 0; i < steps; ++i) {
            world->Step(stepper.GetConfig().fixedDelta);
        }
        world->SetInterpolationAlpha(stepper.GetInterpolationAlpha());

        pendingEvents.clear();
        if (steps > 0) {
            world->DrainEvents(pendingEvents);
        }
        for (auto *listener : listeners) {
            listener->OnPhysicsEvents(pendingEvents);
        }
    }

    void PhysicsSystem::SetOptions(const PhysicsOptions &options)
    {
        if (world != nullptr) {
            world->SetOptions(options);
        }
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
            if (body == nullptr) {
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
