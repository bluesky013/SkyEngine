//
// Created by blues on 2024/8/11.
//

#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationTrackData.h>

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
        if (!position.times.empty()) {
            trans.translation = AnimSampleChannel(position, param);
        }
        if (!scale.times.empty()) {
            trans.scale = AnimSampleChannel(scale, param);
        }
        if (!rotation.times.empty()) {
            trans.rotation = AnimSampleChannel(rotation, param);
        }
    }

    void AnimationNodeChannel::BuildTrackData(AnimationTrackData& out, uint16_t bone) const
    {
        out.AddChannelData(bone, position, scale, rotation);
    }
} // namespace sky
