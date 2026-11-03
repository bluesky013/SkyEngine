//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationChannel.h>
#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <core/util/Uuid.h>

namespace sky {
    struct AnimPose;
    class AnimationTrackData;
    class Skeleton;

    class AnimationClip : public RefObject {
    public:
        explicit AnimationClip(const Name &inName) : name(inName)
        {
        } // NOLINT
        ~AnimationClip() override = default;

        AnimationStatus AddChannel(const AnimChannelPtr &channel);

        AnimationStatus SamplePose(AnimPose &pose, const SampleParam &param) const;

        AnimationStatus BuildTrackData(AnimationTrackData &out, const Skeleton &skeleton) const;
        AnimationStatus Validate(const Skeleton *skeleton = nullptr) const;

        FORCEINLINE void SetNumFrame(uint32_t num)
        {
            sourceFrameCount = num;
        };
        FORCEINLINE void SetFrameRate(float fate)
        {
            frameRate = fate;
        };

        uint32_t GetFrameCount() const;

        FORCEINLINE float GetDuration() const
        {
            return frameRate > 0.f ? static_cast<float>(GetFrameCount()) / frameRate : 0.f;
        }
        FORCEINLINE float GetPlayRate() const
        {
            return frameRate;
        }

        FORCEINLINE const Name &GetName() const
        {
            return name;
        }

    private:
        Name                                     name;
        uint32_t                                 sourceFrameCount = 0;
        float                                    frameRate        = 0.f;
        std::unordered_map<Name, AnimChannelPtr> channels;
    };
    using AnimClipPtr = CounterPtr<AnimationClip>;

} // namespace sky
