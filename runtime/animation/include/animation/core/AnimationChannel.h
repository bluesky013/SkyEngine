//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <core/math/Transform.h>
#include <vector>

namespace sky {

    class AnimationChannel : public RefObject {
    public:
        explicit AnimationChannel(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationChannel() override = default;

        virtual void Sample(const SampleParam &param, Transform &trans) = 0;

        inline float GetDuration() const { return duration; }
        inline const Name& GetName() const { return name; }
    protected:
        Name name;
        float duration;
    };

    using AnimChannelPtr = CounterPtr<AnimationChannel>;

} // namespace sky
