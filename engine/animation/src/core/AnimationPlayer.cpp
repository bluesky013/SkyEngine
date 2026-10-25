//
// Created by blues on 2026/1/1.
//

#include <algorithm>
#include <animation/core/AnimationPlayer.h>
#include <cmath>

namespace sky {

    AnimationStatus AnimationSequencePlayer::SetPlaying(bool play)
    {
        if (play && !clip) {
            return {AnimationError::MissingClip, "player has no clip"};
        }
        playing = play;
        return {};
    }

    void AnimationSequencePlayer::SetLoop(bool loop)
    {
        looping = loop;
    }

    AnimationStatus AnimationSequencePlayer::SetClip(const AnimClipPtr &inClip)
    {
        if (inClip) {
            const auto status = inClip->Validate();
            if (!status) {
                return status;
            }
        }
        clip = inClip;
        Reset();
        return {};
    }

    AnimationStatus AnimationSequencePlayer::SetPlayRate(float rate)
    {
        if (!std::isfinite(rate) || rate < 0.f) {
            return {AnimationError::InvalidPlayback, "play rate must be finite and nonnegative"};
        }
        playRate = rate;
        return {};
    }

    AnimationStatus AnimationSequencePlayer::Seek(float time)
    {
        if (!clip) {
            return {AnimationError::MissingClip, "player has no clip"};
        }
        if (!std::isfinite(time)) {
            return {AnimationError::InvalidPlayback, "seek time must be finite"};
        }
        currentTime = std::clamp(time, 0.f, clip->GetDuration());
        return {};
    }

    void AnimationSequencePlayer::Stop()
    {
        currentTime = 0.f;
        playing     = false;
    }

    void AnimationSequencePlayer::Reset()
    {
        Stop();
        playRate = 1.f;
    }

    AnimationStatus AnimationSequencePlayer::Tick(float deltaTime)
    {
        return Advance(deltaTime);
    }

    AnimationStatus AnimationSequencePlayer::Advance(float delta)
    {
        if (!std::isfinite(delta) || delta < 0.f) {
            return {AnimationError::InvalidPlayback, "animation delta must be finite and nonnegative"};
        }
        if (!clip) {
            return {AnimationError::MissingClip, "player has no clip"};
        }
        if (!playing || delta == 0.f || playRate == 0.f) {
            return {};
        }
        const float duration = clip->GetDuration();
        if (duration <= 0.f) {
            currentTime = 0.f;
            playing     = false;
            return {};
        }
        const double next = static_cast<double>(currentTime) + static_cast<double>(playRate) * delta;
        if (looping) {
            currentTime = static_cast<float>(std::fmod(next, static_cast<double>(duration)));
            if (currentTime >= duration) {
                currentTime = 0.f;
            }
        } else {
            currentTime = static_cast<float>(std::min(next, static_cast<double>(duration)));
            if (currentTime >= duration) {
                playing = false;
            }
        }
        return {};
    }

} // namespace sky
