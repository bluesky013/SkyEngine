//
// Created by blues on 2026/1/1.
//

#pragma once

#include <animation/core/AnimationClip.h>

namespace sky {

    class AnimationSequencePlayer {
    public:
        AnimationSequencePlayer()  = default;
        ~AnimationSequencePlayer() = default;

        AnimationStatus SetPlaying(bool play);
        void            SetLoop(bool loop);

        AnimationStatus SetClip(const AnimClipPtr &inClip);
        AnimationStatus SetPlayRate(float rate);
        AnimationStatus Seek(float time);
        void            Stop();

        AnimationStatus Tick(float deltaTime);
        AnimationStatus Advance(float deltaTime);

        FORCEINLINE float GetCurrentTime() const
        {
            return currentTime;
        }
        FORCEINLINE bool IsPlaying() const
        {
            return playing;
        }

    private:
        void Reset();

        AnimClipPtr clip;

        float currentTime = 0.f;
        float playRate    = 1.f;

        bool playing = false;
        bool looping = false;
    };

} // namespace sky
