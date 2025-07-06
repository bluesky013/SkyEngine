//
// Created by Zach Lee on 2025/6/15.
//

#include <animation/skeleton/SkeletonAnimation.h>
#include <animation/core/AnimationClip.h>

namespace sky {

    SkeletonAnimation::SkeletonAnimation(const SkeletonPtr& inSkeleton)
        : skeleton(inSkeleton)
    {
        currentPose = new Pose();
        currentPose->transforms = skeleton->GetRefPos()->transforms;
        currentPose->skeleton = skeleton.Get();
    }

    void SkeletonAnimation::Sample(AnimationClip& clip, float timePoint)
    {
        clip.SamplePose(*currentPose, timePoint);
    }

} // namespace sky
