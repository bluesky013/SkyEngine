//
// Created by blues on 2024/8/15.
//

#include <animation/core/AnimationInterpolation.h>

namespace sky {

    Quaternion AnimSphericalLinear(const Quaternion &q1, const Quaternion &q2, float t)
    {
        float d = q1.Dot(q2);
        float sign = FloatSelect(d, 1.f, -1.f);
        d *= sign;

        const float theta = std::acos(d);
        const float sinTheta = std::sin(theta);

        Quaternion res = {};
        if (sinTheta < 1e-6f) {
            float k1 = 1 - t;
            float k2 = sign * t;

            res = Quaternion{
                q1.w * k1 + q2.w * k2,
                q1.x * k1 + q2.x * k2,
                q1.y * k1 + q2.y * k2,
                q1.z * k1 + q2.z * k2,
            };

        } else {
            float k1 = std::sin(theta * (1 - t)) / sinTheta;
            float k2 = std::sin(theta * t) / sinTheta * sign;

            res = Quaternion{
                    q1.w * k1 + q2.w * k2,
                    q1.x * k1 + q2.x * k2,
                    q1.y * k1 + q2.y * k2,
                    q1.z * k1 + q2.z * k2,
            };
        }

        res.Normalize();
        return res;
    }

    Quaternion AccumulateShortest(const Quaternion &vk1, const Quaternion& vk2)
    {
        float dot = vk1.Dot(vk2);
        return dot >= 0 ? Quaternion(vk1.x + vk2.x, vk1.y + vk2.y, vk1.z + vk2.z, vk1.w + vk2.w) :
            Quaternion(vk1.x - vk2.x, vk1.y - vk2.y, vk1.z - vk2.z, vk1.w - vk2.w);
    }

} // namespace sky
