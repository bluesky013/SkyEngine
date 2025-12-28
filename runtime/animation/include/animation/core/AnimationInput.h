//
// Created by blues on 2025/12/28.
//

#pragma once

#include <core/util/TimeBlend.h>

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

} // namespace sky