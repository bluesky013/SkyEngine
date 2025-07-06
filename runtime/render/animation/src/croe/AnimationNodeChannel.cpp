//
// Created by blues on 2024/8/11.
//

#include <animation/core/AnimationNodeChannel.h>

namespace sky {

    AnimationNodeChannel::AnimationNodeChannel(const AnimNodeChannelData &data)
        : AnimationChannel(Name(data.name.c_str()))
        , position(data.position)
        , scale(data.scale)
        , rotation(data.rotation)
    {
        if (!position.times.empty())
        {
            duration = std::max(duration, position.times.back());
        }

        if (!scale.times.empty())
        {
            duration = std::max(duration, scale.times.back());
        }

        if (!rotation.times.empty())
        {
            duration = std::max(duration, rotation.times.back());
        }
    }

    void AnimationNodeChannel::Sample(const SampleParam &param, const AnimSampleResultPtr &ptr)
    {

    }
} // namespace sky
