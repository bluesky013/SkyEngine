//
// Created on 2026/10/26.
//

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <core/math/Transform.h>

#include <cmath>

namespace sky::phy {

    inline JPH::Vec3 ToJolt(const Vector3 &v)
    {
        return {v.x, v.y, v.z};
    }
    inline Vector3 FromJolt(JPH::Vec3Arg v)
    {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }
    inline JPH::Quat ToJolt(const Quaternion &q)
    {
        return {q.x, q.y, q.z, q.w};
    }
    inline Quaternion FromJolt(JPH::QuatArg q)
    {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }
    inline bool Finite(float value)
    {
        return std::isfinite(value);
    }
    inline bool Finite(const Vector3 &v)
    {
        return Finite(v.x) && Finite(v.y) && Finite(v.z);
    }
    inline bool ValidTransform(const Transform &t)
    {
        const auto &q = t.rotation;
        return Finite(t.translation) && Finite(t.scale) && t.scale.x > 0 && t.scale.y > 0 && t.scale.z > 0 && Finite(q.x) && Finite(q.y) &&
               Finite(q.z) && Finite(q.w) && std::abs(q.Dot(q) - 1.f) < 0.001f;
    }

} // namespace sky::phy
