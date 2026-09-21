//
// Created by blues on 2026/9/21.
//

#include <animation/acl/AnimationCompressedClip.h>
#include <animation/core/AnimationTrackData.h>

#include <algorithm>
#include <utility>

#if SKY_ANIMATION_ACL

#include <acl/compression/compress.h>
#include <acl/compression/compression_settings.h>
#include <acl/compression/output_stats.h>
#include <acl/compression/track.h>
#include <acl/compression/track_array.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/error_result.h>
#include <acl/core/iallocator.h>
#include <acl/core/track_desc.h>

#include <core/logger/Logger.h>

#include <rtm/quatf.h>
#include <rtm/qvvf.h>
#include <rtm/vector4f.h>

#include <cstring>

namespace sky {

    namespace {

        static const char* TAG = "Animation";

        acl::ansi_allocator& GetAclAllocator()
        {
            static acl::ansi_allocator allocator;
            return allocator;
        }

        acl::qvvf_matrix3x4f_transform_error_metric& GetTransformErrorMetric()
        {
            static acl::qvvf_matrix3x4f_transform_error_metric metric;
            return metric;
        }

        void ToRtm(const Transform& in, rtm::qvvf& out)
        {
            out.rotation = rtm::quat_set(in.rotation.x, in.rotation.y, in.rotation.z, in.rotation.w);
            out.translation = rtm::vector_set(in.translation.x, in.translation.y, in.translation.z, 0.f);
            out.scale = rtm::vector_set(in.scale.x, in.scale.y, in.scale.z, 0.f);
        }

        uint32_t DeriveNumSamples(const AnimationTrackData& tracks)
        {
            uint32_t numSamples = 1;
            const std::vector<AnimTimeKey>& times = tracks.GetTimes();

            for (const AnimTrack& track : tracks.GetTracks()) {
                if (track.keyCount == 0 || track.timeOffset + track.keyCount > times.size()) {
                    continue;
                }
                const AnimTimeKey last = times[track.timeOffset + track.keyCount - 1];
                numSamples = std::max(numSamples, static_cast<uint32_t>(last) + 1);
            }
            return numSamples;
        }

    } // namespace

    bool AnimationClipCompressor::Compress(const AnimationTrackData& tracks, uint32_t numBones, float frameRate,
                                           const Settings& settings, AnimationCompressedClip& out)
    {
        if (numBones == 0 || frameRate <= 0.f || tracks.Empty()) {
            return false;
        }

        const uint32_t numSamples = DeriveNumSamples(tracks);

        // Resample the sparse tracks into a dense per-bone QVV sample block.
        std::vector<rtm::qvvf> samples(static_cast<size_t>(numSamples) * numBones);
        std::vector<Transform> pose(numBones);

        for (uint32_t frame = 0; frame < numSamples; ++frame) {
            std::fill(pose.begin(), pose.end(), Transform::GetIdentity());

            SampleParam param = {};
            param.frameTime = AnimFrameTime{static_cast<AnimTimeKey>(frame), 0.f};
            param.interpolation = AnimInterpolation::LINEAR;
            tracks.SamplePose(pose.data(), param);

            for (uint32_t bone = 0; bone < numBones; ++bone) {
                ToRtm(pose[bone], samples[static_cast<size_t>(frame) * numBones + bone]);
            }
        }

        acl::iallocator& allocator = GetAclAllocator();
        acl::track_array_qvvf trackList(allocator, numBones);

        const uint32_t stride = static_cast<uint32_t>(sizeof(rtm::qvvf)) * numBones;
        for (uint32_t bone = 0; bone < numBones; ++bone) {
            acl::track_desc_transformf desc;
            desc.output_index = bone;
            desc.parent_index = acl::k_invalid_track_index;
            desc.precision = settings.errorThreshold;

            trackList[bone] = acl::track_qvvf::make_copy(desc, allocator, samples.data() + bone, numSamples, frameRate, stride);
        }

        acl::compression_settings compressionSettings = acl::get_default_compression_settings();
        compressionSettings.level = static_cast<acl::compression_level8>(std::min<uint32_t>(settings.compressionLevel, 3));
        compressionSettings.error_metric = &GetTransformErrorMetric();

        acl::compressed_tracks* compressed = nullptr;
        acl::output_stats stats;
        const acl::error_result result = acl::compress_track_list(allocator, trackList, compressionSettings, compressed, stats);

        if (!result.empty() || compressed == nullptr) {
            LOG_E(TAG, "ACL compression failed: %s", result.empty() ? "unknown" : result.c_str());
            return false;
        }

        const size_t size = compressed->get_size();
        std::vector<uint8_t> blob(size);
        std::memcpy(blob.data(), compressed, size);
        allocator.deallocate(compressed, size);

        out.SetBlob(std::move(blob), numBones, numSamples, frameRate);
        return true;
    }

    bool AnimationClipCompressor::Compress(const AnimationTrackData& tracks, uint32_t numBones, float frameRate,
                                           AnimationCompressedClip& out)
    {
        return Compress(tracks, numBones, frameRate, Settings{}, out);
    }

} // namespace sky

#else // SKY_ANIMATION_ACL

namespace sky {

    bool AnimationClipCompressor::Compress(const AnimationTrackData& tracks, uint32_t numBones, float frameRate,
                                           const Settings& settings, AnimationCompressedClip& out)
    {
        (void)tracks;
        (void)numBones;
        (void)frameRate;
        (void)settings;
        (void)out;
        return false;
    }

    bool AnimationClipCompressor::Compress(const AnimationTrackData& tracks, uint32_t numBones, float frameRate,
                                           AnimationCompressedClip& out)
    {
        return Compress(tracks, numBones, frameRate, Settings{}, out);
    }

} // namespace sky

#endif // SKY_ANIMATION_ACL
