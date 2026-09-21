//
// Created by blues on 2024/8/1.
//

#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationTrackData.h>
#include <animation/core/Skeleton.h>
#include <algorithm>

namespace sky {

    void AnimationClip::AddChannel(const AnimChannelPtr &channel)
    {
        channels.emplace(channel->GetName(), channel);
    }

    uint32_t AnimationClip::GetFrameCount() const
    {
        AnimTimeKey lastKey = -1;
        for (const auto& [channelName, channel] : channels) {
            lastKey = std::max(lastKey, channel->GetLastKeyTime());
        }

        if (lastKey >= 0) {
            return static_cast<uint32_t>(lastKey + 1);
        }
        return frameNum;
    }

    void AnimationClip::BuildTrackData(AnimationTrackData& out, const Skeleton& skeleton) const
    {
        for (const auto& [channelName, channel] : channels) {
            const Bone* bone = skeleton.GetBoneByName(channelName);
            if (bone != nullptr) {
                channel->BuildTrackData(out, bone->index);
            }
        }
    }

    void AnimationClip::SamplePose(AnimPose & pose, const SampleParam& param)
    {
        for (auto& [channelName, channel] : channels) {
            const auto *bone = pose.skeleton->GetBoneByName(channelName);
            if (bone != nullptr && pose.boneMask.CheckBit(bone->index)) {
                channel->Sample(param, pose.transforms[bone->index]);
            }
        }
    }

} // namespace sky
