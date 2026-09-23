//
// Created on 2026/09/23.
//

#pragma once

#include <physics/IPhysicsBackend.h>
#include <physics/PhysicsEvents.h>
#include <physics/PhysicsStepping.h>
#include <physics/IPhysicsSystem.h>
#include <physics/PhysicsWorldStats.h>

#include <framework/world/World.h>

#include <memory>
#include <string_view>
#include <vector>

namespace sky::phy {

    // World sub-system that owns an engine physics world and drives it through a fixed-step stepper.
    // Consumers resolve it by name and only see the engine IPhysicsWorld / PhysicsObjectId contract.
    class PhysicsSystem : public IPhysicsSystem {
    public:
        static constexpr std::string_view NAME = PHYSICS_SYSTEM_NAME;

        PhysicsSystem()           = default;
        ~PhysicsSystem() override = default;

        bool Init(const PhysicsWorldDesc &desc = {});

        IPhysicsWorld *GetWorld() const { return world.get(); }

        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;

        void StartSimulation() override;
        void StopSimulation() override;
        void Tick(float time) override;

        void SetSimulating(bool value) override;
        bool IsSimulating() const override;
        void SingleStep() override;

        const PhysicsStepConfig &GetStepConfig() const { return stepper.GetConfig(); }
        void SetStepConfig(const PhysicsStepConfig &config) { stepper.SetConfig(config); }

        // Runtime-configurable physics options (mobile budgets, determinism experiments, authoring).
        void           SetOptions(const PhysicsOptions &options);
        PhysicsOptions GetOptions() const;

        // Listener-based dispatch (called each Tick after stepping) and a polling alternative.
        void AddEventListener(IPhysicsEventListener *listener);
        void RemoveEventListener(IPhysicsEventListener *listener);
        void DrainEvents(std::vector<PhysicsEvent> &out);

        PhysicsWorldStats GetStats() const;

        void GatherCollisionMeshes(std::vector<CollisionMeshInstance> &out) const override;

    private:
        std::unique_ptr<IPhysicsWorld> world;
        PhysicsStepper                 stepper;
        bool                           simulating = true;
        World                         *attachedWorld = nullptr;

        std::vector<IPhysicsEventListener *> listeners;
        std::vector<PhysicsEvent>            pendingEvents;
    };

    // Creates a physics world from the registered backend, wraps it in a PhysicsSystem and attaches it
    // to the world. Returns nullptr when no backend is registered.
    PhysicsSystem *AttachPhysicsSystem(World &world, const PhysicsWorldDesc &desc = {});

} // namespace sky::phy
