//
// Created by blues on 2024/8/11.
//

#include <animation/core/AnimationNodeChannel.h>

namespace sky {

    AnimationNodeChannel::AnimationNodeChannel(const AnimNodeChannelData &data)
        : AnimationChannel(data.name)
        , position(data.position)
        , scale(data.scale)
        , rotation(data.rotation)
    {
    }

    void AnimationNodeChannel::Sample(const SampleParam &param, const AnimSampleResultPtr &ptr)
    {

    }
} // namespace sky