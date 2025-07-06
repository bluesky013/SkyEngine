//
// Created by blues on 2024/8/1.
//

#include <animation/core/AnimationClip.h>

namespace sky {

    void AnimationClip::AddChannel(const AnimChannelPtr &channel)
    {
        duration = std::max(duration, channel->GetDuration());
        channels.emplace_back(channel);
    }

} // namespace sky
