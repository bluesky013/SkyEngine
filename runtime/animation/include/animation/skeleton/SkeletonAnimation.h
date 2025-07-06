//
// Created by Zach Lee on 2025/6/15.
//

#pragma once

#include <animation/core/Animation.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {

    class SkeletonAnimation : public KeyFrameAnimation {
    public:
        explicit SkeletonAnimation(const SkeletonPtr& skeleton);
        ~SkeletonAnimation() override = default;
        void Sample(AnimationClip& clip, float timePoint) override;

        const PosePtr &GetCurrentPose() const { return currentPose; }
        const SkeletonPtr &GetSkeleton() const { return skeleton; }
    private:
        SkeletonPtr skeleton;
        PosePtr currentPose;
    };

} // namespace sky
