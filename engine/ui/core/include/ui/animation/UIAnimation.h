//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace sky::ui {

    enum class UIEasing : uint8_t {
        LINEAR = 0,
        EASE_IN_QUAD,
        EASE_OUT_QUAD,
        EASE_IN_OUT_QUAD,
        EASE_OUT_CUBIC,
    };

    float ApplyEasing(UIEasing easing, float t);

    struct UIFloatKeyframe {
        float time = 0.0f;
        float value = 0.0f;
    };

    class UIFloatTrack {
    public:
        void AddKey(float time, float value);
        float Evaluate(float time) const;
        bool IsEmpty() const { return keys.empty(); }

    private:
        std::vector<UIFloatKeyframe> keys;
    };

    // Duration-based tween driven by UIContext::Tick.
    class UIAnimation {
    public:
        using UpdateFn = std::function<void(float)>;

        UIAnimation(float duration, UpdateFn update, UIEasing easing = UIEasing::LINEAR);

        void SetOnComplete(std::function<void()> callback) { onComplete = std::move(callback); }
        void SetLoop(bool value) { loop = value; }

        float GetTime() const { return time; }
        float GetDuration() const { return duration; }
        bool IsComplete() const { return complete; }

        // Advances the animation; returns true while it is still active.
        bool Advance(float delta);

    private:
        float duration = 0.0f;
        float time = 0.0f;
        UIEasing easing = UIEasing::LINEAR;
        bool loop = false;
        bool complete = false;
        UpdateFn update;
        std::function<void()> onComplete;
    };

} // namespace sky::ui
