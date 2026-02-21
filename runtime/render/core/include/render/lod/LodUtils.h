//
// Created by blues on 2026/2/16.
//

#pragma once

#include <core/shapes/AABB.h>
#include <render/lod/LodTypes.h>

namespace sky {

    float CalculateScreenSizeByIndex(float lodIndex);

    float CalculateScreenSizeByBound(const Vector3 &origin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj);

    float CalculateScreenSizeSquired(const Vector3 &origin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj);
} // namespace sky
