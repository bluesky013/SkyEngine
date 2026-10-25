//
// Created by blues on 2025/12/28.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <cmath>
#include <core/util/TimeBlend.h>
#include <limits>

namespace sky {

    struct AnimFadeInOut {
        float fadeInTime;
        float fadeOutTime;
        bool  initialized;

        TimeBlend blend;

        explicit AnimFadeInOut(float time);

        float Eval(float deltaTime, bool onActive);

        void SetBlendTime(float time)
        {
            fadeInTime  = time;
            fadeOutTime = time;
        }

        void Reset();
    };

    namespace Anim {

        FORCEINLINE bool IsFullWeight(float weight)
        {
            return weight >= ANIM_INV_BLEND_WEIGHT_THRESHOLD;
        }

        FORCEINLINE bool IsRelevant(float weight)
        {
            return weight > ANIM_BLEND_WEIGHT_THRESHOLD;
        }

        FORCEINLINE AnimFrameTime ConvertFromFrameRate(float inSeconds, float framePerSeconds)
        {
            const double frame = static_cast<double>(inSeconds) * framePerSeconds;
            if (!std::isfinite(frame)) {
                return {0, 0.f};
            }
            const double clamped = std::clamp(frame, 0.0, static_cast<double>(std::numeric_limits<AnimTimeKey>::max()));
            const double whole   = std::floor(clamped);
            return {static_cast<AnimTimeKey>(whole), static_cast<float>(clamped - whole)};
        }

    } // namespace Anim

} // namespace sky
