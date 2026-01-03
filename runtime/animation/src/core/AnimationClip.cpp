//
// Created by blues on 2024/8/1.
//

#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationPose.h>
#include <animation/core/Skeleton.h>

namespace sky {

    void AnimationClip::AddChannel(const AnimChannelPtr &channel)
    {
        channels.emplace(channel->GetName(), channel);
    }

    void AnimationClip::SamplePose(AnimPose & pose, const SampleParam& param)
    {
        for (auto& [channelName, channel] : channels) {
            const auto *bone = pose.skeleton->GetBoneByName(channelName);
            if (bone != nullptr) {
                channel->Sample(param, pose.transforms[bone->index]);
            }
        }
    }

} // namespace sky
