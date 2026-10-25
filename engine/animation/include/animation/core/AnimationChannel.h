//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationStatus.h>
#include <animation/core/AnimationTypes.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    class AnimationTrackData;

    class AnimationChannel : public RefObject {
    public:
        explicit AnimationChannel(const Name &name_) : name(name_)
        {
        } // NOLINT
        ~AnimationChannel() override = default;

        virtual void Sample(const SampleParam &param, Transform &trans) = 0;

        FORCEINLINE const Name &GetName() const
        {
            return name;
        }

        virtual AnimTimeKey GetLastKeyTime() const
        {
            return -1;
        }
        virtual AnimationStatus Validate() const
        {
            return {};
        }

        virtual void BuildTrackData(AnimationTrackData &out, uint16_t bone) const
        {
            (void)out;
            (void)bone;
        }

    protected:
        Name name;
    };

    using AnimChannelPtr = CounterPtr<AnimationChannel>;

} // namespace sky
