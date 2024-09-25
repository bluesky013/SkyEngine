//
// Created by blues on 2024/8/1.
//

#pragma once

#include <vector>

namespace sky {

    enum class Interpolation {
        LINEAR,
        STEP,
        CUBIC_SPLINE,
    };

    struct SampleParam {
        float timePoint = 0.f;
        Interpolation interpolation;
    };

    template <typename T>
    struct AnimChannelData {
        std::vector<float> time;
        std::vector<T> keys;
    };
} // namespace sky