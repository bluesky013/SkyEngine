//
// Created on 2026/09/23.
//

#include <bullet/BulletRuntime.h>

#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>

#include <framework/world/World.h>

namespace sky::phy {

    PhysicsWorld *AttachPhysicsWorld(World &world)
    {
        auto *physics = PhysicsRegistry::Get()->CreatePhysicsWorld();
        if (physics != nullptr) {
            world.AddSubSystem(Name(PhysicsWorld::NAME.data()), physics);
        }
        return physics;
    }

} // namespace sky::phy
