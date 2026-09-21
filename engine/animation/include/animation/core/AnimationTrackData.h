//
// Created by blues on 2026/9/21.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationTypes.h>
#include <core/math/Transform.h>

#include <cstdint>
#include <vector>

namespace sky {

    enum class AnimTrackComponent : uint8_t {
        Translation = 0,
        Scale       = 1,
        Rotation    = 2,
    };

    struct AnimTrack {
        uint16_t           bone          = 0;
        AnimTrackComponent component     = AnimTrackComponent::Translation;
        AnimInterpolation  interpolation = AnimInterpolation::LINEAR;
        uint32_t           timeOffset    = 0;
        uint32_t           valueOffset   = 0;
        uint32_t           keyCount      = 0;
    };

    /**
     * Structure-of-arrays keyframe storage. Times and values live in separate,
     * fixed-stride contiguous blocks so the layout is coalesced and uploadable.
     */
    class AnimationTrackData {
    public:
        void Clear();

        bool Empty() const { return tracks.empty(); }
        uint32_t GetNumTracks() const { return static_cast<uint32_t>(tracks.size()); }
        const AnimTrack& GetTrack(uint32_t index) const { return tracks[index]; }

        void AddChannelData(uint16_t bone,
                            const AnimChannelData<Vector3>& position,
                            const AnimChannelData<Vector3>& scale,
                            const AnimChannelData<Quaternion>& rotation);

        void SampleTrack(uint32_t trackIndex, const SampleParam& param, Transform& out) const;
        void SamplePose(Transform* pose, const SampleParam& param, const AnimationBoneMask* mask = nullptr) const;

        const std::vector<AnimTrack>& GetTracks() const { return tracks; }
        const std::vector<AnimTimeKey>& GetTimes() const { return times; }
        const std::vector<Vector3>& GetTranslations() const { return translations; }
        const std::vector<Vector3>& GetScales() const { return scales; }
        const std::vector<Quaternion>& GetRotations() const { return rotations; }

    private:
        friend struct AnimationPlanBlob;

        void AddVec3Track(uint16_t bone, AnimTrackComponent comp, AnimInterpolation interp,
                          const std::vector<AnimTimeKey>& inTimes, const std::vector<Vector3>& inValues);
        void AddRotationTrack(uint16_t bone, AnimInterpolation interp,
                              const std::vector<AnimTimeKey>& inTimes, const std::vector<Quaternion>& inValues);

        std::vector<AnimTrack> tracks;
        std::vector<AnimTimeKey> times;
        std::vector<Vector3> translations;
        std::vector<Vector3> scales;
        std::vector<Quaternion> rotations;
    };

} // namespace sky
