//
// Created by blues on 2026/1/1.
//

#include <algorithm>
#include <animation/core/AnimationPlayer.h>

namespace sky {

    void AnimationSequencePlayer::SetPlaying(bool play)
    {
        playing = play;
    }

    void AnimationSequencePlayer::SetLoop(bool loop)
    {
        looping = loop;
    }

    void AnimationSequencePlayer::SetClip(const AnimClipPtr& inClip)
    {
        clip = inClip;
        Reset();
    }

    void AnimationSequencePlayer::SetPlayRate(float rate)
    {
        playRate = rate;
    }

    void AnimationSequencePlayer::Reset()
    {
        currentTime = 0.f;
        playRate    = 1.f;
        playing     = false;

    }

    float AnimationSequencePlayer::Tick(float deltaTime)
    {
        float outDelta = Advance(deltaTime);

        if (!looping && currentTime >= clip->GetDuration()) {
            playing = false;
        }

        return outDelta;
    }

    float AnimationSequencePlayer::Advance(float delta)
    {
        float deltaTime = playRate * delta;
        currentTime = AdvanceTime(looping, deltaTime, clip->GetDuration(), currentTime);

        return deltaTime;
    }

    float AnimationSequencePlayer::AdvanceTime(bool loop, float delta, float length, float current)
    {
        float time = current + delta;
        if (time < 0.f || time > length) {
            time = loop ? ((length == 0.f) ? std::fmod(time, length) : 0.f)
                        : std::clamp(time, 0.f, length);
        }
        return time;
    }

} // namespace sky