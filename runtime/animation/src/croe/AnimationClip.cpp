//
// Created by blues on 2024/8/1.
//

#include <animation/core/AnimationClip.h>
#include <animation/skeleton/Pose.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {

    void AnimationClip::AddChannel(const AnimChannelPtr &channel)
    {
        duration = std::max(duration, channel->GetDuration());
        channels.emplace(channel->GetName(), channel);
    }

    void AnimationClip::SamplePose(Pose& pose, float time)
    {
        SampleParam param = {};
        param.timePoint = time;
        param.interpolation = AnimInterpolation::LINEAR;

        for (auto& [name, channel] : channels) {
            auto *bone = pose.skeleton->GetBoneByName(name);
            SKY_ASSERT(bone != nullptr);

            channel->Sample(param,  pose.transforms[bone->index]);
        }
    }

} // namespace sky
