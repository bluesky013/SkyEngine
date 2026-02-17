//
// Created by blues on 2025/12/28.
//

#pragma once

#include <cstdint>

namespace sky {

    class TimeBlend {
    public:
        enum class BlendOption : uint8_t {
            LINEAR = 0
        };

        explicit TimeBlend(float blendTime, BlendOption op = BlendOption::LINEAR);
        ~TimeBlend() = default;

        void Reset();

        void SetAlpha(float alpha);

        void SetDesiredValue(float desired);

        void SetBlendTime(float time);

        void Update(float deltaTime);

        float GetAlpha() const { return blendedValue; }

        float GetDesiredValue() const { return desiredValue; }

    private:
        void ResetAlpha();
        void ResetBlendTime();

        float blendTime;
        float remainingTime;

        float alpha;
        float beginValue;
        float desiredValue;
        float blendedValue;

        BlendOption option;

        bool resetAlpha;
        bool resetBlendTime;
    };

} // namespace sky