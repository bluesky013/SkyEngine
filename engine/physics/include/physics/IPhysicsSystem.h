//
// Created on 2026/09/23.
//

#pragma once

#include <physics/IPhysicsBackend.h>

#include <core/math/Transform.h>
#include <core/shapes/TriangleMesh.h>

#include <framework/world/World.h>

#include <memory>
#include <vector>

namespace sky::phy {

    class IPhysicsStepListener {
    public:
        virtual ~IPhysicsStepListener()                                            = default;
        virtual PhysicsStatus BeforePhysicsStep(IPhysicsWorld &world, float delta) = 0;
        virtual PhysicsStatus AfterPhysicsStep(IPhysicsWorld &world, float delta)  = 0;
        virtual void          OnPhysicsWorldClosing(IPhysicsWorld &world)          = 0;
    };

    // A local-space collision mesh together with its world transform, for nav mesh generation.
    struct CollisionMeshInstance {
        CounterPtr<TriangleMesh> mesh;
        Transform                transform;
    };

    // Engine seam for hosts (launcher/editor) and other subsystems: exposes the physics runtime and the
    // editor-facing simulation/debug surface without linking a physics backend plugin. Resolved through
    // the world by PHYSICS_SYSTEM_NAME and down-cast to this interface.
    class IPhysicsSystem : public IWorldSubSystem {
    public:
        virtual IPhysicsWorld *GetWorld() const = 0;

        // Simulation control for the editor (play/pause/single-step/replay drive these).
        virtual void SetSimulating(bool simulating) = 0;
        virtual bool IsSimulating() const = 0;
        virtual void SingleStep() = 0;

        // Host-thread only. Listeners are owned until removed; callbacks may unregister themselves.
        virtual PhysicsStatus AddStepListener(std::shared_ptr<IPhysicsStepListener> listener)
        {
            return {PhysicsError::Unsupported, "Fixed-step listeners are unsupported"};
        }
        virtual void RemoveStepListener(const IPhysicsStepListener *listener)
        {
        }
        virtual float GetInterpolationAlpha() const
        {
            return 1.f;
        }

        // Navigation consumes collision geometry through this call (engine types only).
        virtual void GatherCollisionMeshes(std::vector<CollisionMeshInstance> &out) const = 0;
    };

} // namespace sky::phy
