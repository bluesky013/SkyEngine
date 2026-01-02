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

        void SamplePose(AnimPose & pose, const SampleParam& param);

        FORCEINLINE void SetNumFrame(uint32_t num) { frameNum = num; };
        FORCEINLINE void SetFrameRate(float fate) { frameRate = fate; };

        FORCEINLINE float GetDuration() const { return frameRate > 0.f ? static_cast<float>(frameNum) / frameRate : 0.f; }
        FORCEINLINE float GetPlayRate() const { return frameRate; }

        FORCEINLINE const Name& GetName() const { return name; }
    private:
        Name name;
        uint32_t frameNum = 0;
        float frameRate = 0.f;
        std::unordered_map<Name, AnimChannelPtr> channels;
    };
    using AnimClipPtr = CounterPtr<AnimationClip>;

} // namespace sky
