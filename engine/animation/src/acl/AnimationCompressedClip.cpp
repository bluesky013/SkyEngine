//
// Created by blues on 2026/9/21.
//

#include <animation/acl/AnimationCompressedClip.h>

#include <algorithm>

#if SKY_ANIMATION_ACL

#include <acl/core/compressed_tracks.h>
#include <acl/core/sample_rounding_policy.h>
#include <acl/core/track_writer.h>
#include <acl/decompression/decompress.h>
#include <acl/decompression/decompression_settings.h>

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

namespace sky {

    namespace {

        /**
         * Writes ACL decompressed samples into our Transform layout.
         * ACL calls write_rotation/translation/scale per track when writing a qvvf track list.
         */
        struct PoseWriter final : public acl::track_writer {
            Transform* pose = nullptr;

            void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation)
            {
                const rtm::vector4f v = rtm::quat_to_vector(rotation);
                pose[track_index].rotation = Quaternion(
                    rtm::vector_get_w(v), rtm::vector_get_x(v), rtm::vector_get_y(v), rtm::vector_get_z(v));
            }

            void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation)
            {
                pose[track_index].translation = Vector3(
                    rtm::vector_get_x(translation), rtm::vector_get_y(translation), rtm::vector_get_z(translation));
            }

            void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale)
            {
                pose[track_index].scale = Vector3(
                    rtm::vector_get_x(scale), rtm::vector_get_y(scale), rtm::vector_get_z(scale));
            }
        };

    } // namespace

    AnimationCompressedClip::~AnimationCompressedClip() = default;

    AnimationCompressedClip::AnimationCompressedClip(AnimationCompressedClip&& other) noexcept = default;
    AnimationCompressedClip& AnimationCompressedClip::operator=(AnimationCompressedClip&& other) noexcept = default;

    void AnimationCompressedClip::SetBlob(std::vector<uint8_t> inBlob, uint32_t inNumBones, uint32_t inNumSamples, float inFrameRate)
    {
        blob = std::move(inBlob);
        numBones = inNumBones;
        numSamples = inNumSamples;
        frameRate = inFrameRate;
        duration = frameRate > 0.f ? static_cast<float>(numSamples) / frameRate : 0.f;
    }

    void AnimationCompressedClip::Clear()
    {
        blob.clear();
        blob.shrink_to_fit();
        numBones = 0;
        numSamples = 0;
        frameRate = 0.f;
        duration = 0.f;
    }

    bool AnimationCompressedClip::Sample(float time, Transform* pose) const
    {
        if (blob.empty() || pose == nullptr || numBones == 0) {
            return false;
        }

        const acl::compressed_tracks* tracks = acl::make_compressed_tracks(blob.data());
        if (tracks == nullptr || !tracks->is_valid(false).empty()) {
            return false;
        }

        acl::decompression_context<acl::decompression_settings> context;
        if (!context.initialize(*tracks)) {
            return false;
        }

        const float clamped = std::clamp(time, 0.f, duration);
        context.seek(clamped, acl::sample_rounding_policy::none);

        PoseWriter writer;
        writer.pose = pose;
        context.decompress_tracks(writer);
        return true;
    }

    std::string AnimationCompressedClip::Dump() const
    {
        std::string out = "AnimationCompressedClip bones=" + std::to_string(numBones)
            + " samples=" + std::to_string(numSamples)
            + " frameRate=" + std::to_string(frameRate)
            + " duration=" + std::to_string(duration)
            + " bytes=" + std::to_string(blob.size());
        return out;
    }

} // namespace sky

#else // SKY_ANIMATION_ACL

namespace sky {

    AnimationCompressedClip::~AnimationCompressedClip() = default;
    AnimationCompressedClip::AnimationCompressedClip(AnimationCompressedClip&& other) noexcept = default;
    AnimationCompressedClip& AnimationCompressedClip::operator=(AnimationCompressedClip&& other) noexcept = default;

    void AnimationCompressedClip::SetBlob(std::vector<uint8_t> inBlob, uint32_t inNumBones, uint32_t inNumSamples, float inFrameRate)
    {
        blob = std::move(inBlob);
        numBones = inNumBones;
        numSamples = inNumSamples;
        frameRate = inFrameRate;
        duration = frameRate > 0.f ? static_cast<float>(numSamples) / frameRate : 0.f;
    }

    void AnimationCompressedClip::Clear()
    {
        blob.clear();
        numBones = 0;
        numSamples = 0;
        frameRate = 0.f;
        duration = 0.f;
    }

    bool AnimationCompressedClip::Sample(float time, Transform* pose) const
    {
        (void)time;
        (void)pose;
        return false;
    }

    std::string AnimationCompressedClip::Dump() const
    {
        return "AnimationCompressedClip (ACL disabled)";
    }

} // namespace sky

#endif // SKY_ANIMATION_ACL
