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
    }

    void AnimationNodeChannel::Sample(const SampleParam &param, Transform& trans)
    {
        trans.translation = AnimSampleChannel(position, param);
        trans.scale = AnimSampleChannel(scale, param);
        trans.rotation = AnimSampleChannel(rotation, param);
    }
} // namespace sky
