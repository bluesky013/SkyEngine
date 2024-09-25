//
// Created by blues on 2024/8/30.
//

#pragma once

#include <animation/core/Animation.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationClip.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {

    class SkeletonAnimation : public Animation {
    public:
        SkeletonAnimation() = default;
        ~SkeletonAnimation() override = default;

    private:
        void Tick(float delta) override;

        AnimClipPtr clip;
        SkeletonPtr skeleton;
    };

} // namespace sky