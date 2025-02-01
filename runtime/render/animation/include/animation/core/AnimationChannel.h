//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <vector>

namespace sky {

    struct AnimSampleResult : RefObject {
        AnimSampleResult() = default;
        ~AnimSampleResult() override = default;
    };
    using AnimSampleResultPtr = CounterPtr<AnimSampleResult>;

    class AnimationChannel : public RefObject {
    public:
        explicit AnimationChannel(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationChannel() override = default;

        virtual void Sample(const SampleParam &param, const AnimSampleResultPtr &ptr) = 0;
    protected:
        Name name;
    };

    using AnimChannelPtr = CounterPtr<AnimationChannel>;

} // namespace sky
