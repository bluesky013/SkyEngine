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
        return vk * (1 - t)  + vk1 * t;
    }

    Quaternion AnimSphericalLinear(const Quaternion &vk, const Quaternion &vk1, float t);

    Quaternion AccumulateShortest(const Quaternion &vk1, const Quaternion& vk2);

    template <typename T>
    T AnimSampleChannel(const AnimChannelData<T> &data, const SampleParam &param)
    {
        SKY_ASSERT(data.times.size() == data.keys.size());
        SKY_ASSERT(!data.times.empty());

        auto [t1, t2] = data.FindKeyFrame(param.timePoint);

        if (t1 == t2) {
            return data.keys[t1];
        }

        // const T &vk1 = data.keys[t1];
        // const T &vk2 = data.keys[t2];
        // float t = (param.timePoint - data.times[t1]) / (data.times[t2] - data.times[t1]);
        //
        // switch (param.interpolation) {
        // case AnimInterpolation::STEP:
        //     return vk1;
        // case AnimInterpolation::CUBIC_SPLINE: // not supported yet.
        // case AnimInterpolation::LINEAR:
        //     if constexpr (std::is_same_v<T, Quaternion>) {
        //         return AnimSphericalLinear(vk1, vk2, t);
        //     } else {
        //         return AnimInterpolateLinear(vk1, vk2, t);
        //     }
        // }
        return T{};
    }

} // namespace sky
