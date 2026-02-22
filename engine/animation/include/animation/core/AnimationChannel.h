//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <core/math/Transform.h>

namespace sky {

    class AnimationChannel : public RefObject {
    public:
        explicit AnimationChannel(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationChannel() override = default;

        virtual void Sample(const SampleParam &param, Transform &trans) = 0;

        FORCEINLINE const Name& GetName() const { return name; }
    protected:
        Name name;
    };

    using AnimChannelPtr = CounterPtr<AnimationChannel>;

} // namespace sky
