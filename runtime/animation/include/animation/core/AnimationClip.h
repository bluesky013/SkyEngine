//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationChannel.h>

namespace sky {
    struct AnimPose;

    class AnimationClip : public RefObject {
    public:
        explicit AnimationClip(const Name &name_) : name(name_) {} // NOLINT
        ~AnimationClip() override = default;

        void AddChannel(const AnimChannelPtr &channel);

        void SamplePose(AnimPose & pose, float time);

        FORCEINLINE float GetDuration() const { return duration; }
        FORCEINLINE const Name& GetName() const { return name; }
        FORCEINLINE bool IsLooping() const { return isLooping; }
    private:
        Name name;
        bool isLooping = false;
        float duration = 0.f;
        std::unordered_map<Name, AnimChannelPtr> channels;
    };
    using AnimClipPtr = CounterPtr<AnimationClip>;

} // namespace sky
