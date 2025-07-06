//
// Created by blues on 2024/8/2.
//

#include <animation/core/Animation.h>

namespace sky {

    void Animation::AddClip(const AnimClipPtr& clip)
    {
        SKY_ASSERT(clip && "clip should be empty")
        clips.emplace_back(clip);
    }

    void KeyFrameAnimation::Tick(float delta)
    {
        if (currentClip >= clips.size()) {
            return;
        }
        auto &clip = clips[currentClip];

        currentTime += delta;
        float duration = clip->GetDuration();
        if (currentTime > duration) {
            currentTime = clip->IsLooping() ? std::fmod(currentTime, duration) : duration;
        }

        Sample(*clip, currentTime);
    }

} // namespace sky
