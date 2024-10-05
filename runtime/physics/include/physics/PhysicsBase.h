//
// Created by blues on 2024/10/3.
//

#pragma once

#include <framework/interface/IMeshConfigNotify.h>
#include <core/math/Vector3.h>

namespace sky::phy {

    struct SphereShape {
        Vector3 pivot = VEC3_ZERO;
        float radius = 1.f;
    };

    struct BoxShape {
        Vector3 pivot = VEC3_ZERO;
        Vector3 scale = VEC3_ONE;
    };

    struct MeshPhysicsConfig : public MeshConfigBase {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
    };

} // namespace sky::phy