//
// Created by blues on 2024/10/3.
//

#pragma once

#include <framework/interface/IMeshConfigNotify.h>
#include <core/math/Vector3.h>

namespace sky::phy {

    struct SphereShape {
        Vector3 pivot;
        float radius;
    };

    struct BoxShape {
        Vector3 pivot;
        Vector3 scale;
    };

    struct MeshPhysicsConfig : public MeshConfigBase {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
    };

} // namespace sky::phy