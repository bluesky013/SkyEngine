//
// Created by Zach Lee on 2023/8/13.
//

#pragma once

#include <array>
#include <core/shapes/Base.h>
#include <core/math/Matrix4.h>

namespace sky {

    struct Frustum {
        // left, right, bottom, top, near, far
        std::array<Plane, 6> planes;
    };

    Frustum CreateFrustumByViewProjectMatrix(const Matrix4 &mtx);

} // namespace sky
