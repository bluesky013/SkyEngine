//
// Created on 2026/10/07.
//

#pragma once

#include <framework/serialization/SerializationContext.h>

namespace sky::phy {

    // Authoring config for the physics world subsystem (shown in the editor's
    // project-level Config panel).
    struct PhysicsSubSystemConfig {
        float gravity             = -9.81f;     // acceleration along +Y (m/s^2)
        float fixedTimestep       = 0.0166667f; // 60 Hz
        int   maxSubSteps         = 4;
        int   solverIterations    = 10;
        float sleepThreshold      = 0.005f;
        float collisionMargin     = 0.04f;
        bool  deterministic       = false;
        bool  continuousCollision = false;

        static void Reflect(SerializationContext *context);
    };

} // namespace sky::phy
