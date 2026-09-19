//
// Created on 2026/09/19.
//

#include <ui/animation/UIAnimation.h>

#include <algorithm>
#include <cmath>

namespace sky::ui {

    float ApplyEasing(UIEasing easing, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (easing) {
        case UIEasing::EASE_IN_QUAD:
            return t * t;
        case UIEasing::EASE_OUT_QUAD:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case UIEasing::EASE_IN_OUT_QUAD:
            return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case UIEasing::EASE_OUT_CUBIC: {
            const float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }
        case UIEasing::LINEAR:
        default:
            return t;
        }
    }

    void UIFloatTrack::AddKey(float time, float value)
    {
        UIFloatKeyframe key;
        key.time = time;
        key.value = value;

        const auto it = std::lower_bound(keys.begin(), keys.end(), key, [](const UIFloatKeyframe &a, const UIFloatKeyframe &b) {
            return a.time < b.time;
        });
        keys.insert(it, key);
    }

    float UIFloatTrack::Evaluate(float time) const
    {
        if (keys.empty()) {
            return 0.0f;
        }
        if (time <= keys.front().time) {
            return keys.front().value;
        }
        if (time >= keys.back().time) {
            return keys.back().value;
        }

        for (size_t i = 1; i < keys.size(); ++i) {
            if (time <= keys[i].time) {
                const UIFloatKeyframe &a = keys[i - 1];
                const UIFloatKeyframe &b = keys[i];
                const float span = b.time - a.time;
                const float t = span > 0.0f ? (time - a.time) / span : 0.0f;
                return a.value + (b.value - a.value) * t;
            }
        }
        return keys.back().value;
    }

    UIAnimation::UIAnimation(float duration, UpdateFn update, UIEasing easing)
        : duration(duration)
        , easing(easing)
        , update(std::move(update))
    {
    }

    bool UIAnimation::Advance(float delta)
    {
        if (complete) {
            return false;
        }

        time += delta;

        if (loop && duration > 0.0f && time >= duration) {
            time = std::fmod(time, duration);
        }

        const float t = duration > 0.0f ? std::clamp(time / duration, 0.0f, 1.0f) : 1.0f;
        if (update) {
            update(ApplyEasing(easing, t));
        }

        if (!loop && time >= duration) {
            complete = true;
            if (onComplete) {
                onComplete();
            }
            return false;
        }
        return true;
    }

} // namespace sky::ui
