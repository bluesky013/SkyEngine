//
// Created by Zach Lee on 2025/6/15.
//

#include <animation/skeleton/SkeletonAnimation.h>
#include <animation/core/AnimationClip.h>

namespace sky {

    SkeletonAnimation::SkeletonAnimation(const SkeletonPtr& inSkeleton)
        : skeleton(inSkeleton)
    {
    }

    void SkeletonAnimation::Sample(AnimationClip& clip, float timePoint)
    {
    }

} // namespace sky
