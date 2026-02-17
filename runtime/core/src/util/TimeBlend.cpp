//
// Created by blues on 2025/12/28.
//

#include <core/util/TimeBlend.h>
#include <algorithm>

namespace sky {

    static float ResolveAlphaBlendByBlendOption(float inAlpha, TimeBlend::BlendOption /*op*/)
    {
        return inAlpha;
    }

    TimeBlend::TimeBlend(float inTime, BlendOption op)
        : blendTime(inTime)
        , beginValue(0.f)
        , desiredValue(1.f)
        , option(op)
    {
        Reset();
    }

    void TimeBlend::SetAlpha(float inAlpha)
    {
        alpha = std::clamp(inAlpha, 0.f, 1.f);
        blendedValue = beginValue + (desiredValue - beginValue) * ResolveAlphaBlendByBlendOption(alpha, option);
    }

    void TimeBlend::SetDesiredValue(float desired)
    {
        beginValue = blendedValue;
        desiredValue = desired;

        resetAlpha = true;
    }

    void TimeBlend::SetBlendTime(float time)
    {
        blendTime = std::max(0.f, time);
        resetBlendTime = true;
    }

    void TimeBlend::Reset()
    {
        if (blendTime <= 0.f) {
            SetAlpha(1.f);
            remainingTime = 0.f;
        } else {
            SetAlpha(0.f);
            remainingTime = blendTime * std::abs(1.f - alpha);
        }

        resetAlpha = false;
        resetBlendTime = false;
    }

    void TimeBlend::ResetAlpha()
    {
        if (beginValue == desiredValue) {
            SetAlpha(1.f);
        } else {
            alpha = (blendedValue - beginValue)/(desiredValue - beginValue);
            SetAlpha(alpha);
        }

        resetAlpha = false;
    }

    void TimeBlend::ResetBlendTime()
    {
        if (blendTime <= 0.f) {
            remainingTime = 0.f;
            SetAlpha(1.f);
        } else {
            remainingTime = blendTime * std::abs(1.f - alpha);
        }

        resetBlendTime = false;
    }

    void TimeBlend::Update(float deltaTime)
    {
        if (resetAlpha) {
            ResetAlpha();
        }

        if (resetBlendTime) {
            ResetBlendTime();
        }

        if (remainingTime > deltaTime) {
            alpha += (1.f - alpha) / remainingTime * deltaTime;
            remainingTime -= deltaTime;
            SetAlpha(alpha);
        } else {
            remainingTime = 0.f;
            SetAlpha(1.f);
        }
    }

} // namespace sky