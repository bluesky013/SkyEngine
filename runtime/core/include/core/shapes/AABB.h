//
// Created by Zach Lee on 2023/8/29.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>

namespace sky {

    struct AABB {
        Vector3 min;
        Vector3 max;

        static AABB Transform(const AABB& box, const Matrix4 &matrix);
    };

    void Merge(const AABB &a, const AABB &b, AABB &out);
} // namespace sky
