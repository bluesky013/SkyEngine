//
// Created by blues on 2024/8/10.
//

#pragma once

#include <animation/core/AnimationChannel.h>
#include <animation/core/AnimationInterpolation.h>
#include <core/math/Vector3.h>
#include <core/math/Quaternion.h>
#include <core/name/Name.h>

#include <string>
#include <utility>

namespace sky {

    struct AnimNodeChannelData {
        std::string name;
        AnimChannelData<Vector3> position;
        AnimChannelData<Vector3> scale;
        AnimChannelData<Quaternion> rotation;
    };

    class AnimationNodeChannel : public AnimationChannel {
    public:
        explicit AnimationNodeChannel(const Name &name) : AnimationChannel(name) {}
        explicit AnimationNodeChannel(const AnimNodeChannelData &data_);

        ~AnimationNodeChannel() override = default;

        void Sample(const SampleParam &param, const AnimSampleResultPtr &ptr) override;
    private:
        AnimChannelData<Vector3> position;
        AnimChannelData<Vector3> scale;
        AnimChannelData<Quaternion> rotation;
    };
    using AnimNodeChannelPtr = CounterPtr<AnimationNodeChannel>;

} // namespace sky
