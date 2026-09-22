//
// Created on 2026/09/22.
//

#pragma once

#include <core/math/Vector3.h>

namespace sky::phy {

    class CollisionObject;

    // Backend-neutral query results (no backend handles/types).
    struct RaycastHit {
        CollisionObject *object = nullptr;
        Vector3          position;
        Vector3          normal;
        float            distance = 0.f;
    };

    struct SweepResult {
        CollisionObject *object = nullptr;
        Vector3          position;
        Vector3          normal;
        float            distance = 0.f;
    };

    struct OverlapResult {
        CollisionObject *object = nullptr;
    };

} // namespace sky::phy
