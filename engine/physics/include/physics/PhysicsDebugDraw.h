//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsDebugGeometry.h>

namespace sky::phy {

    // Render-agnostic physics debug interface: emits plain geometry, no render resource types.
    class PhysicsDebugDraw {
    public:
        PhysicsDebugDraw() = default;
        virtual ~PhysicsDebugDraw() = default;

        virtual void CollectGeometry(PhysicsDebugGeometry &out) const = 0;
    };

} // namespace sky::phy
