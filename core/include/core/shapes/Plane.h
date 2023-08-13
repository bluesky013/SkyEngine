//
// Created by Zach Lee on 2023/8/13.
//

#pragma once

#include <core/math/Vector3.h>

namespace sky {

    struct Plane {
        Vector3 normal = VEC3_Y;
        float distance = 0;
    };

    Plane CreatePlaneByVertices(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3);
    Plane CreatePlaneByNormalAndVertex(const Vector3 &normal, const Vector3 &pt);

} // namespace sky