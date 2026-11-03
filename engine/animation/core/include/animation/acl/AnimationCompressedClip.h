//
// Created by blues on 2026/9/21.
//

#pragma once

#include <animation/core/AnimationTypes.h>
#include <core/math/Transform.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky {

    class AnimationTrackData;

    /**
     * ACL2 (Animation Compression Library) compressed animation clip.
     *
     * Cook time:  AnimationClipCompressor::Compress(...)
     * Runtime:    Sample(time, pose) -- decompression only.
     *
     * The blob is a serialized `acl::compressed_tracks` buffer and is pointer-free,
     * so it can be stored alongside the other runtime animation assets.
     */
    class AnimationCompressedClip {
    public:
        AnimationCompressedClip() = default;
        ~AnimationCompressedClip();

        AnimationCompressedClip(AnimationCompressedClip&& other) noexcept;
        AnimationCompressedClip& operator=(AnimationCompressedClip&& other) noexcept;
        AnimationCompressedClip(const AnimationCompressedClip&) = delete;
        AnimationCompressedClip& operator=(const AnimationCompressedClip&) = delete;

        bool IsValid() const { return !blob.empty(); }

        float GetDuration() const { return duration; }
        float GetFrameRate() const { return frameRate; }
        uint32_t GetNumBones() const { return numBones; }
        uint32_t GetNumSamples() const { return numSamples; }
        const std::vector<uint8_t>& GetBlob() const { return blob; }

        void SetBlob(std::vector<uint8_t> inBlob, uint32_t inNumBones, uint32_t inNumSamples, float inFrameRate);
        void Clear();

        /** Decompresses every bone at `time` into pose[0..numBones). */
        bool Sample(float time, Transform* pose) const;

        std::string Dump() const;

    private:
        friend class AnimationClipCompressor;

        std::vector<uint8_t> blob;
        uint32_t numBones = 0;
        uint32_t numSamples = 0;
        float frameRate = 0.f;
        float duration = 0.f;
    };

    /**
     * Cook-time compression of animation tracks into an ACL2 clip.
     */
    class AnimationClipCompressor {
    public:
        struct Settings {
            float errorThreshold = 0.01f;   // bounded reconstruction error
            uint8_t compressionLevel = 1;   // 0=low, 1=medium, 2=high, 3=highest
        };

        static bool Compress(const AnimationTrackData& tracks,
                             uint32_t numBones,
                             float frameRate,
                             const Settings& settings,
                             AnimationCompressedClip& out);

        static bool Compress(const AnimationTrackData& tracks,
                             uint32_t numBones,
                             float frameRate,
                             AnimationCompressedClip& out);
    };

} // namespace sky
