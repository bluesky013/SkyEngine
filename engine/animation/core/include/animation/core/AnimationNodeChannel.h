//
// Created by blues on 2024/8/10.
//

#pragma once

#include <animation/core/AnimationChannel.h>
#include <animation/core/AnimationInterpolation.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector3.h>
#include <core/name/Name.h>

#include <string>
#include <utility>

namespace sky {

    struct AnimNodeChannelData {
        std::string                 name;
        AnimChannelData<Vector3>    position;
        AnimChannelData<Vector3>    scale;
        AnimChannelData<Quaternion> rotation;

        AnimationStatus Validate() const;
        uint32_t        GetFrameCount() const;

        AnimationStatus Compress()
        {
            const auto status = Validate();
            if (!status) {
                return status;
            }
            position.Compress();
            scale.Compress();
            rotation.Compress();
            return {};
        }
    };

    class AnimationNodeChannel : public AnimationChannel {
    public:
        explicit AnimationNodeChannel(const Name &name) : AnimationChannel(name)
        {
        }
        explicit AnimationNodeChannel(const AnimNodeChannelData &data_);

        ~AnimationNodeChannel() override = default;

        void            Sample(const SampleParam &param, Transform &trans) override;
        AnimationStatus Validate() const override;

        AnimTimeKey GetLastKeyTime() const override
        {
            AnimTimeKey last   = -1;
            const auto  update = [&last](const auto &data) {
                if (!data.times.empty() && data.times.back() > last) {
                    last = data.times.back();
                }
            };

            update(position);
            update(scale);
            update(rotation);
            return last;
        }

        void BuildTrackData(AnimationTrackData &out, uint16_t bone) const override;

    private:
        AnimChannelData<Vector3>    position;
        AnimChannelData<Vector3>    scale;
        AnimChannelData<Quaternion> rotation;
    };
    using AnimNodeChannelPtr = CounterPtr<AnimationNodeChannel>;

} // namespace sky
