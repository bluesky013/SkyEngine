//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/name/Name.h>
#include <vector>

namespace sky {

    enum class AnimInterpolation {
        LINEAR,
        STEP,
        CUBIC_SPLINE,
    };

    struct SampleParam {
        float timePoint = 0.f;
        AnimInterpolation interpolation;
    };

    template <typename T>
    struct AnimChannelData {
        std::pair<size_t, size_t> FindKeyFrame(float time) const
        {
            auto it = std::upper_bound(
                    times.begin(),
                    times.end(),
                    time,
                    [](float t1, float t2) { return t1 < t2; });

            if (it == times.begin()) return {0, 0};

            size_t last = times.size() - 1;
            if (it == times.end()) return { last, last };

            size_t idx = std::distance(times.begin(), it);
            return { idx - 1, idx };
        }

        std::vector<float> times;
        std::vector<T> keys;
    };


    enum class AnimBlendMode : uint8_t {
        Override,
        Additive,
        Multiply,
        Replace
    };

    enum class AnimLayerUpdateMode : uint8_t {
        Always,
        WhenActive,
        Manual
    };

    enum class AnimParamType : uint8_t {
        UNKNOWN = 0,
        BOOL,
        UINT,
        INT,
        FLOAT,
        DOUBLE,
        STR
    };

    enum class AnimComp : uint8_t {
        NEV = 0,
        LT = 1,
        EQ = 2,
        LE = 3,
        GT = 4,
        NE = 5,
        GE = 6,
        AWS = 7,
    };

    static constexpr uint32_t ANIM_INVALID = ~(0U);
} // namespace sky
