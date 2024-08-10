//
// Created by blues on 2024/8/15.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/MathUtil.h>

namespace sky {

    /*
     * https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.pdf : Appendix C
     */

    template <typename T>
    T AnimInterpolateLinear(const T &vk, const T &vk1, float t)
    {
        return (1 - t) * vk + t * vk1;
    }

    Quaternion AnimSphericalLinear(const Quaternion &vk, const Quaternion &vk1, float t);

    template <typename T>
    T AnimSampleChannel(const AnimChannelData<T> &data, const SampleParam &param)
    {
        SKY_ASSERT(data.time.size() == data.keys.size());
        SKY_ASSERT(!data.time.empty());

        uint32_t k = 0;
        for (k = 0; k < data.time.size(); ++k) {
            if (param.timePoint < data.time[k]) {
                break;
            }
        }

        const T vk1 = {};
        const T vk2 = {};
        float t = 0.f;

        switch (param.interpolation) {
            case Interpolation::STEP:
                return vk1;
                break;
            case Interpolation::CUBIC_SPLINE: // not supported yet.
            case Interpolation::LINEAR:
                if constexpr (std::is_same_v<T, Quaternion>) {
                    return SphericalLinear(vk1, vk2, t);
                } else {
                    return AnimInterpolateLinear(vk1, vk2, t);
                }
        }
    }

} // namespace sky