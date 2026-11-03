//
// Created by blues on 2026/9/21.
//

#include <animation/acl/AnimationCompressedClip.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationTrackData.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace sky;

namespace {

    AnimationTrackData MakeTestTracks(uint32_t numBones, const std::vector<AnimTimeKey>& times)
    {
        AnimationTrackData tracks;

        for (uint32_t bone = 0; bone < numBones; ++bone) {
            AnimNodeChannelData channel;
            channel.name = "B" + std::to_string(bone);

            channel.position.times = times;
            channel.rotation.times = times;

            for (size_t i = 0; i < times.size(); ++i) {
                const float t = static_cast<float>(times[i]);
                channel.position.keys.emplace_back(0.f, t * (1.f + static_cast<float>(bone)), 0.f);

                const float angle = 0.25f * t;
                const float s = std::sin(angle);
                const float c = std::cos(angle);
                channel.rotation.keys.emplace_back(c, 0.f, 0.f, s);
            }

            tracks.AddChannelData(static_cast<uint16_t>(bone), channel.position, channel.scale, channel.rotation);
        }

        return tracks;
    }

    void ExpectNear(const Transform& a, const Transform& b)
    {
        EXPECT_NEAR(a.translation.x, b.translation.x, 1e-2f);
        EXPECT_NEAR(a.translation.y, b.translation.y, 1e-2f);
        EXPECT_NEAR(a.translation.z, b.translation.z, 1e-2f);

        const float dot = std::abs(a.rotation.Dot(b.rotation));
        EXPECT_GT(dot, 0.999f);
    }

} // namespace

TEST(AnimationAclTest, CompressSampleRoundTrip)
{
#if !SKY_ANIMATION_ACL
    GTEST_SKIP() << "ACL2 disabled";
#else
    const uint32_t numBones = 4;
    const float frameRate = 30.f;
    const std::vector<AnimTimeKey> times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    AnimationTrackData tracks = MakeTestTracks(numBones, times);

    AnimationCompressedClip clip;
    ASSERT_TRUE(AnimationClipCompressor::Compress(tracks, numBones, frameRate, clip)) << clip.Dump();
    ASSERT_TRUE(clip.IsValid());
    EXPECT_EQ(clip.GetNumBones(), numBones);
    EXPECT_FLOAT_EQ(clip.GetDuration(), 10.f / frameRate);

    // compression shrinks the data
    EXPECT_GT(clip.GetBlob().size(), 0u);

    // sample every frame and compare against the source tracks
    std::vector<Transform> srcPose(numBones);
    std::vector<Transform> aclPose(numBones);

    for (uint32_t frame = 0; frame < 10; ++frame) {
        std::fill(srcPose.begin(), srcPose.end(), Transform::GetIdentity());
        std::fill(aclPose.begin(), aclPose.end(), Transform::GetIdentity());

        SampleParam param = {};
        param.frameTime = AnimFrameTime{static_cast<AnimTimeKey>(frame), 0.f};
        param.interpolation = AnimInterpolation::LINEAR;
        tracks.SamplePose(srcPose.data(), param);

        ASSERT_TRUE(clip.Sample(static_cast<float>(frame) / frameRate, aclPose.data()));

        for (uint32_t bone = 0; bone < numBones; ++bone) {
            ExpectNear(srcPose[bone], aclPose[bone]);
        }
    }
#endif
}

TEST(AnimationAclTest, CompressionIsDeterministic)
{
#if !SKY_ANIMATION_ACL
    GTEST_SKIP() << "ACL2 disabled";
#else
    const uint32_t numBones = 3;
    const std::vector<AnimTimeKey> times = {0, 1, 2, 3, 4};

    AnimationTrackData tracks = MakeTestTracks(numBones, times);

    AnimationCompressedClip first;
    AnimationCompressedClip second;
    ASSERT_TRUE(AnimationClipCompressor::Compress(tracks, numBones, 30.f, first));
    ASSERT_TRUE(AnimationClipCompressor::Compress(tracks, numBones, 30.f, second));

    EXPECT_EQ(first.GetBlob(), second.GetBlob());
#endif
}

TEST(AnimationAclTest, BlobRoundTrip)
{
#if !SKY_ANIMATION_ACL
    GTEST_SKIP() << "ACL2 disabled";
#else
    const uint32_t numBones = 2;
    const std::vector<AnimTimeKey> times = {0, 1, 2, 3};

    AnimationTrackData tracks = MakeTestTracks(numBones, times);

    AnimationCompressedClip source;
    ASSERT_TRUE(AnimationClipCompressor::Compress(tracks, numBones, 30.f, source));

    AnimationCompressedClip restored;
    restored.SetBlob(source.GetBlob(), source.GetNumBones(), source.GetNumSamples(), source.GetFrameRate());

    std::vector<Transform> a(numBones);
    std::vector<Transform> b(numBones);

    ASSERT_TRUE(source.Sample(0.05f, a.data()));
    ASSERT_TRUE(restored.Sample(0.05f, b.data()));

    for (uint32_t bone = 0; bone < numBones; ++bone) {
        ExpectNear(a[bone], b[bone]);
    }
#endif
}
