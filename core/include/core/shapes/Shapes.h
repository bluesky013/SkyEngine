//
// Created by Zach Lee on 2023/8/29.
//

#pragma once

#include <core/shapes/AABB.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/Plane.h>

namespace sky {

    bool Intersection(const AABB &lhs, const AABB &rhs);
    bool Intersection(const AABB &aabb, const Frustum &frustum);
    std::pair<bool, int> Intersection(const AABB &aabb, const Plane &plane);

} // namespace sky
