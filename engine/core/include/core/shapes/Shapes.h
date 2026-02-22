//
// Created by Zach Lee on 2023/8/29.
//

#pragma once

#include <core/shapes/AABB.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/Base.h>
#include <core/shapes/Bounds.h>

namespace sky {
    // intersection
    bool Intersection(const AABB &lhs, const AABB &rhs);
    bool Intersection(const AABB &aabb, const Frustum &frustum);
    std::pair<bool, int> Intersection(const AABB &aabb, const Plane &plane);

    // utils
    Plane CreatePlaneByVertices(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3);
    Plane CreatePlaneByNormalAndVertex(const Vector3 &normal, const Vector3 &pt);

    float DistanceToPlane(const Vector3 &pt, const Plane &plane);

    std::pair<bool, Vector3> CalculateInterSection(const Ray &ray, const Plane &plane);

} // namespace sky
