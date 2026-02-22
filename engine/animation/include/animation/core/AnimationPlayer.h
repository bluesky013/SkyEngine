//
// Created by blues on 2026/1/1.
//

#pragma once

#include <animation/core/AnimationClip.h>

namespace sky {

    class AnimationSequencePlayer {
    public:
        AnimationSequencePlayer() = default;
        ~AnimationSequencePlayer() = default;

        void SetPlaying(bool play);
        void SetLoop(bool loop);

        void SetClip(const AnimClipPtr& inClip);
        void SetPlayRate(float rate);

        float Tick(float deltaTime);
        float Advance(float deltaTime);

        FORCEINLINE float GetCurrentTime() const { return currentTime; }
        FORCEINLINE bool IsPlaying() const { return playing; }

        static float AdvanceTime(bool loop, float delta, float length, float current);
    private:
        void Reset();

        AnimClipPtr clip;

        float currentTime = 0.f;
        float playRate = 1.f;

        bool playing = false;
        bool looping = false;
    };


} // namespace sky
