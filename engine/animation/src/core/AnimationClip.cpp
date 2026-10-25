//
// Created by blues on 2024/8/1.
//

#include <algorithm>
#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationTrackData.h>
#include <animation/core/Skeleton.h>
#include <cmath>

namespace sky {

    AnimationStatus AnimationClip::AddChannel(const AnimChannelPtr &channel)
    {
        if (!channel || channel->GetName().GetStr().empty()) {
            return {AnimationError::InvalidChannel, "channel requires a name and data"};
        }
        const auto status = channel->Validate();
        if (!status) {
            return status;
        }
        if (!channels.emplace(channel->GetName(), channel).second) {
            return {AnimationError::InvalidChannel, "duplicate clip channel"};
        }
        return {};
    }

    uint32_t AnimationClip::GetFrameCount() const
    {
        if (sourceFrameCount != 0) {
            return sourceFrameCount;
        }
        AnimTimeKey lastKey = -1;
        for (const auto &[channelName, channel] : channels) {
            lastKey = std::max(lastKey, channel->GetLastKeyTime());
        }

        if (lastKey >= 0) {
            return static_cast<uint32_t>(lastKey) + 1U;
        }
        return 0;
    }

    AnimationStatus AnimationClip::Validate(const Skeleton *skeleton) const
    {
        if (!std::isfinite(frameRate) || frameRate < 0.f || !std::isfinite(GetDuration())) {
            return {AnimationError::InvalidDuration, "clip frame rate and duration must be finite and nonnegative"};
        }
        for (const auto &[channelName, channel] : channels) {
            if (sourceFrameCount != 0 && channel->GetLastKeyTime() >= 0 && static_cast<uint32_t>(channel->GetLastKeyTime()) >= sourceFrameCount) {
                return {AnimationError::InvalidDuration, "source frame count does not cover the last key"};
            }
            if (skeleton != nullptr && skeleton->GetBoneByName(channelName) == nullptr) {
                return {AnimationError::InvalidChannel, "clip channel is missing from the skeleton: " + std::string(channelName.GetStr())};
            }
        }
        return {};
    }

    AnimationStatus AnimationClip::BuildTrackData(AnimationTrackData &out, const Skeleton &skeleton) const
    {
        const auto status = Validate(&skeleton);
        if (!status) {
            return status;
        }
        out.Clear();
        for (const auto &[channelName, channel] : channels) {
            const Bone *bone = skeleton.GetBoneByName(channelName);
            if (bone != nullptr) {
                channel->BuildTrackData(out, bone->index);
            }
        }
        return {};
    }

    AnimationStatus AnimationClip::SamplePose(AnimPose &pose, const SampleParam &param) const
    {
        if (pose.skeleton == nullptr || !std::isfinite(param.frameTime.subFrame)) {
            return {AnimationError::InvalidSkeleton, "sampling requires a skeleton and finite sample time"};
        }
        const auto status = Validate(pose.skeleton);
        if (!status) {
            return status;
        }
        pose.transforms = pose.skeleton->GetRefPos()->transforms;
        for (const auto &[channelName, channel] : channels) {
            const auto *bone = pose.skeleton->GetBoneByName(channelName);
            if (bone != nullptr && pose.boneMask.CheckBit(bone->index)) {
                channel->Sample(param, pose.transforms[bone->index]);
            }
        }
        return {};
    }

} // namespace sky
