//
// Created by Zach Lee on 2023/8/13.
//

#pragma once

#include <array>
#include <core/shapes/Plane.h>

namespace sky {

    struct Frustum {
        std::array<Plane, 6> planes;
    };

    Frustum CreateByPerspective(float fov, float aspect, float near, float far);

} // namespace sky