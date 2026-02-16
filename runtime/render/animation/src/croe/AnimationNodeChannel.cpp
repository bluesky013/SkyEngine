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

    void AnimationNodeChannel::Sample(const SampleParam &param, const AnimSampleResultPtr &ptr)
    {
        auto *result = static_cast<AnimNodeSampleResult*>(ptr.Get());
        if (result == nullptr) {
            return;
        }

        if (!position.time.empty()) {
            result->position = AnimSampleChannel(position, param);
        }
        if (!scale.time.empty()) {
            result->scale = AnimSampleChannel(scale, param);
        }
        if (!rotation.time.empty()) {
            result->rotation = AnimSampleChannel(rotation, param);
        }
    }
} // namespace sky