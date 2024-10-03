//
// Created by blues on 2024/10/3.
//

#pragma once

#include <framework/interface/IMeshConfigNotify.h>
#include <core/math/Vector3.h>

namespace sky::phy {

    struct SpherePrimConfig {
        Vector3 pivot;
        float radius;
    };

    struct BoxPrimConfig {
        Vector3 pivot;
        Vector3 scale;
    };

    struct MeshPhysicsConfig : public MeshConfigBase {
        std::vector<SpherePrimConfig> sphere;
        std::vector<BoxPrimConfig> box;
    };

} // namespace sky::phy