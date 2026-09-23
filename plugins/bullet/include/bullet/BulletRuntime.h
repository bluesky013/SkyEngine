//
// Created on 2026/09/23.
//

#pragma once

namespace sky {
    class World;
} // namespace sky

namespace sky::phy {

    class PhysicsWorld;

    // Runtime attach path: creates a physics world from the registered backend and attaches it to the
    // given world as a sub-system. Returns the created world (or nullptr if no backend is registered).
    PhysicsWorld *AttachPhysicsWorld(World &world);

} // namespace sky::phy
