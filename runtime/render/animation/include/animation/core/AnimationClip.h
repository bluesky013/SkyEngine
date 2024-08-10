//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/util/Name.h>
#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationChannel.h>

namespace sky {

    class AnimationClip : public RefObject {
    public:
        explicit AnimationClip(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationClip() override = default;

        void AddChannel(const AnimChannelPtr &channel);
    private:
        Name name;
        std::vector<AnimChannelPtr> channels;
    };
    using AnimClipPtr = CounterPtr<AnimationClip>;

} // namespace sky