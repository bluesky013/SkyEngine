//
// Created by blues on 2024/8/11.
//

#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationTrackData.h>
#include <cmath>

namespace sky {

    namespace {
        template <typename T>
        AnimationStatus ValidateTrack(const AnimChannelData<T> &track)
        {
            if (track.times.size() != track.keys.size()) {
                return {AnimationError::InvalidChannel, "track time and value counts differ"};
            }
            AnimTimeKey previous = -1;
            for (size_t i = 0; i < track.times.size(); ++i) {
                if (track.times[i] <= previous) {
                    return {AnimationError::InvalidChannel, "track times must be nonnegative and strictly increasing"};
                }
                previous      = track.times[i];
                double length = 0.0;
                for (size_t axis = 0; axis < VectorTraits<T>::Size; ++axis) {
                    const float value = VectorTraits<T>::Visit(track.keys[i], axis);
                    if (!std::isfinite(value)) {
                        return {AnimationError::InvalidChannel, "track values must be finite"};
                    }
                    length += static_cast<double>(value) * value;
                }
                if constexpr (std::is_same_v<T, Quaternion>) {
                    if (std::abs(length - 1.0) > 1e-4) {
                        return {AnimationError::InvalidChannel, "rotation keys must be unit quaternions"};
                    }
                }
            }
            return {};
        }

        AnimationStatus
        ValidateTracks(const AnimChannelData<Vector3> &position, const AnimChannelData<Vector3> &scale, const AnimChannelData<Quaternion> &rotation)
        {
            const auto positionStatus = ValidateTrack(position);
            if (!positionStatus) {
                return positionStatus;
            }
            const auto scaleStatus = ValidateTrack(scale);
            return scaleStatus ? ValidateTrack(rotation) : scaleStatus;
        }
    } // namespace

    AnimationStatus AnimNodeChannelData::Validate() const
    {
        if (name.empty() || name.find('\0') != std::string::npos) {
            return {AnimationError::InvalidChannel, "channel name must be nonempty and contain no null bytes"};
        }
        return ValidateTracks(position, scale, rotation);
    }

    uint32_t AnimNodeChannelData::GetFrameCount() const
    {
        uint32_t   count  = 0;
        const auto update = [&count](const auto &track) {
            if (!track.times.empty() && track.times.back() >= 0) {
                count = std::max(count, static_cast<uint32_t>(track.times.back()) + 1U);
            }
        };
        update(position);
        update(scale);
        update(rotation);
        return count;
    }

    AnimationStatus AnimationNodeChannel::Validate() const
    {
        return ValidateTracks(position, scale, rotation);
    }

    AnimationNodeChannel::AnimationNodeChannel(const AnimNodeChannelData &data)
        : AnimationChannel(Name(data.name.c_str())), position(data.position), scale(data.scale), rotation(data.rotation)
    {
    }

    void AnimationNodeChannel::Sample(const SampleParam &param, Transform &trans)
    {
        if (!position.times.empty()) {
            trans.translation = AnimSampleChannel(position, param);
        }
        if (!scale.times.empty()) {
            trans.scale = AnimSampleChannel(scale, param);
        }
        if (!rotation.times.empty()) {
            trans.rotation = AnimSampleChannel(rotation, param);
        }
    }

    void AnimationNodeChannel::BuildTrackData(AnimationTrackData &out, uint16_t bone) const
    {
        out.AddChannelData(bone, position, scale, rotation);
    }
} // namespace sky
