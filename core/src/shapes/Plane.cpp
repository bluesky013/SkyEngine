//
// Created by Zach Lee on 2023/8/13.
//

#include <core/shapes/Plane.h>

namespace sky {

    Plane CreatePlaneByVertices(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3)
    {
        const Vector3 d1 = v2 - v1;
        const Vector3 d2 = v3 - v1;

        Vector3 normal = d1.Cross(d2);
        normal.Normalize();
        return CreatePlaneByNormalAndVertex(normal, d1);
    }

    Plane CreatePlaneByNormalAndVertex(const Vector3 &normal, const Vector3 &pt)
    {
        return Plane{normal, normal.Dot(pt)};
    }

} // namespace sky