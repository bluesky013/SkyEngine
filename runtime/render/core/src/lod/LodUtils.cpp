//
// Created by blues on 2026/2/16.
//

#include <render/lod/LodUtils.h>
#include <core/math/MathUtil.h>

namespace sky {

    float CalculateScreenSizeByIndex(float lodIndex)
    {
        return std::pow(0.75f, lodIndex);
    }

    float CalculateScreenSizeByBound(const Vector3 &boundOrigin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj)
    {
        const float dist = Length(boundOrigin - viewOrigin);
        const float screenMultiple = std::max(0.5f * proj.m[0][0], 0.5f * proj.m[1][1]);
        const float screenRadius = screenMultiple * sphereRadius / std::max(1.0f, dist);
        return screenRadius * 2.0f;
    }

    float CalculateScreenSizeSquired(const Vector3 &boundOrigin, float sphereRadius, const Vector3& viewOrigin, const Matrix4& proj)
    {
        // for perspective or orthographic projection
        const float factor = std::abs(proj.m[2][3]);
        const float distSqr = LengthSquared(boundOrigin - viewOrigin) * factor;
        const float screenMultiple = std::max(0.5f * proj.m[0][0], 0.5f * proj.m[1][1]);
        return Square(screenMultiple * sphereRadius) / std::max(1.0f, distSqr);
    }

} // namespace sky