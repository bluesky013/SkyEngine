//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationChannel.h>

namespace sky {

    class AnimationClip : public RefObject {
    public:
        explicit AnimationClip(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationClip() override = default;

        void AddChannel(const AnimChannelPtr &channel);

        inline float GetDuration() const { return duration; }
        inline const Name& GetName() const { return name; }
        inline bool IsLooping() const { return isLooping; }
    private:
        Name name;
        bool isLooping = false;
        float duration = 0.f;
        std::vector<AnimChannelPtr> channels;
    };
    using AnimClipPtr = CounterPtr<AnimationClip>;

} // namespace sky
