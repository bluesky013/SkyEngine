//
// Created by blues on 2025/12/28.
//

#include <animation/core/AnimationInput.h>

namespace sky {

    AnimFadeInOut::AnimFadeInOut(float time)
        : fadeInTime(0.f)
        , fadeOutTime(0.f)
        , initialized(false)
        , blend(time)
    {
    }

    float AnimFadeInOut::Eval(float deltaTime, bool onActive)
    {
        float targetValue = onActive ? 1.f : 0.f;

        if (!initialized) {
            blend.SetDesiredValue(targetValue);
            blend.SetBlendTime(0.f);
            blend.Reset();
            initialized = true;
        } else {
            if (blend.GetDesiredValue() != targetValue)
            {
                blend.SetDesiredValue(targetValue);
                blend.SetBlendTime(onActive ? fadeInTime : fadeOutTime);
            }
        }

        blend.Update(deltaTime);

        return blend.GetAlpha();
    }

    void AnimFadeInOut::Reset()
    {
        initialized = false;
    }

} // namespace sky