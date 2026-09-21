//
// Created by blues on 2026/9/21.
//

#include <animation/core/AnimationTrackData.h>
#include <animation/core/AnimationInterpolation.h>
#include <animation/core/AnimationNodeChannel.h>

#include <algorithm>

namespace sky {

    void AnimationTrackData::Clear()
    {
        tracks.clear();
        times.clear();
        translations.clear();
        scales.clear();
        rotations.clear();
    }

    void AnimationTrackData::AddVec3Track(uint16_t bone, AnimTrackComponent comp, AnimInterpolation interp,
                                          const std::vector<AnimTimeKey>& inTimes, const std::vector<Vector3>& inValues)
    {
        if (inTimes.empty() || inTimes.size() != inValues.size()) {
            return;
        }

        AnimTrack track;
        track.bone = bone;
        track.component = comp;
        track.interpolation = interp;
        track.timeOffset = static_cast<uint32_t>(times.size());
        track.keyCount = static_cast<uint32_t>(inTimes.size());

        if (comp == AnimTrackComponent::Translation) {
            track.valueOffset = static_cast<uint32_t>(translations.size());
            translations.insert(translations.end(), inValues.begin(), inValues.end());
        } else {
            track.valueOffset = static_cast<uint32_t>(scales.size());
            scales.insert(scales.end(), inValues.begin(), inValues.end());
        }

        times.insert(times.end(), inTimes.begin(), inTimes.end());
        tracks.emplace_back(track);
    }

    void AnimationTrackData::AddRotationTrack(uint16_t bone, AnimInterpolation interp,
                                              const std::vector<AnimTimeKey>& inTimes, const std::vector<Quaternion>& inValues)
    {
        if (inTimes.empty() || inTimes.size() != inValues.size()) {
            return;
        }

        AnimTrack track;
        track.bone = bone;
        track.component = AnimTrackComponent::Rotation;
        track.interpolation = interp;
        track.timeOffset = static_cast<uint32_t>(times.size());
        track.valueOffset = static_cast<uint32_t>(rotations.size());
        track.keyCount = static_cast<uint32_t>(inTimes.size());

        times.insert(times.end(), inTimes.begin(), inTimes.end());
        rotations.insert(rotations.end(), inValues.begin(), inValues.end());
        tracks.emplace_back(track);
    }

    void AnimationTrackData::AddChannelData(uint16_t bone,
                                            const AnimChannelData<Vector3>& position,
                                            const AnimChannelData<Vector3>& scale,
                                            const AnimChannelData<Quaternion>& rotation)
    {
        AddVec3Track(bone, AnimTrackComponent::Translation, AnimInterpolation::LINEAR, position.times, position.keys);
        AddVec3Track(bone, AnimTrackComponent::Scale, AnimInterpolation::LINEAR, scale.times, scale.keys);
        AddRotationTrack(bone, AnimInterpolation::LINEAR, rotation.times, rotation.keys);
    }

    void AnimationTrackData::SampleTrack(uint32_t trackIndex, const SampleParam& param, Transform& out) const
    {
        const AnimTrack& track = tracks[trackIndex];
        const uint32_t count = track.keyCount;
        if (count == 0) {
            return;
        }

        uint32_t k1 = 0;
        uint32_t k2 = 0;
        if (count > 1) {
            const auto begin = times.begin() + track.timeOffset;
            const auto end = begin + count;
            const auto it = std::upper_bound(begin, end, param.frameTime.frame);
            if (it == begin) {
                k1 = k2 = 0;
            } else if (it == end) {
                k1 = k2 = count - 1;
            } else {
                k2 = static_cast<uint32_t>(it - begin);
                k1 = k2 - 1;
            }
        }

        float t = 0.f;
        if (k1 != k2) {
            const float t1 = static_cast<float>(times[track.timeOffset + k1]);
            const float t2 = static_cast<float>(times[track.timeOffset + k2]);
            const float denom = t2 - t1;
            t = denom > 0.f ? (static_cast<float>(param.frameTime) - t1) / denom : 0.f;
        }

        const bool step = (param.interpolation == AnimInterpolation::STEP) || (track.interpolation == AnimInterpolation::STEP);

        switch (track.component) {
        case AnimTrackComponent::Translation: {
            const Vector3 v1 = translations[track.valueOffset + k1];
            out.translation = (k1 == k2 || step)
                ? v1
                : AnimInterpolateLinear(v1, translations[track.valueOffset + k2], t);
            break;
        }
        case AnimTrackComponent::Scale: {
            const Vector3 v1 = scales[track.valueOffset + k1];
            out.scale = (k1 == k2 || step)
                ? v1
                : AnimInterpolateLinear(v1, scales[track.valueOffset + k2], t);
            break;
        }
        case AnimTrackComponent::Rotation: {
            const Quaternion v1 = rotations[track.valueOffset + k1];
            out.rotation = (k1 == k2 || step)
                ? v1
                : AnimSphericalLinear(v1, rotations[track.valueOffset + k2], t);
            break;
        }
        }
    }

    void AnimationTrackData::SamplePose(Transform* pose, const SampleParam& param, const AnimationBoneMask* mask) const
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(tracks.size()); ++i) {
            const uint16_t bone = tracks[i].bone;
            if (mask != nullptr && !mask->CheckBit(bone)) {
                continue;
            }
            SampleTrack(i, param, pose[bone]);
        }
    }

} // namespace sky
