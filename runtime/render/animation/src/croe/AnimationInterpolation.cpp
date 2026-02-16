//
// Created by blues on 2024/8/15.
//

#include <animation/core/AnimationInterpolation.h>

namespace sky {

    Quaternion AnimSphericalLinear(const Quaternion &vk, const Quaternion &vk1, float t)
    {
        float d = vk.Dot(vk1);
        float sign = FloatSelect(d, 1.f, -1.f);
        d *= sign;

        float k1 = 1 - t;
        float k2 = sign * t;

        if (d < 0.9999) {
            const float a = acos(sign * d);
            const float invS = 1.f / sin(a);
            k1 = sin(a * (1 - t)) * invS;
            k2 = sin(a * t) * invS * sign;
        }

        return {
                k1 * vk.w + k2 * vk1.w,
                k1 * vk.x + k2 * vk1.x,
                k1 * vk.y + k2 * vk1.y,
                k1 * vk.z + k2 * vk1.z
        };
    }

} // namespace sky