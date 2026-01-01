//
// Created by blues on 2025/12/28.
//

#pragma once

#include <core/util/TimeBlend.h>
#include <animation/core/AnimationTypes.h>

namespace sky {

    struct AnimFadeInOut {
        float fadeInTime;
        float fadeOutTime;
        bool initialized;

        TimeBlend blend;

        explicit AnimFadeInOut(float time);

        float Eval(float deltaTime, bool onActive);

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

    } // namespace Anim

} // namespace sky