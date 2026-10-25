#include <content/adaptor/ContentReflection.h>
#include <content/adaptor/assets/AnimationAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <limits>

using namespace sky;

namespace {
    class MemoryClipAsset : public Asset<AnimationClip> {
    public:
        void Bind(std::vector<uint8_t> value)
        {
            bytes         = std::move(value);
            productStream = new IMemoryArchive(bytes.data(), bytes.size());
            bulkStart     = 0;
        }

    private:
        std::vector<uint8_t> bytes;
    };

    AnimNodeChannelData ConstantChannel()
    {
        AnimNodeChannelData channel;
        channel.name           = "root";
        channel.position.times = {0, 10, 60};
        channel.position.keys  = {Vector3(2.f), Vector3(2.f), Vector3(2.f)};
        return channel;
    }

    std::vector<uint8_t> Source(uint32_t version, uint32_t frames, const AnimNodeChannelData &channel)
    {
        OMemoryArchive      bytes;
        BinaryOutputArchive ar(bytes);
        ar.SaveValue(version);
        ar.SaveValue(std::string("constant"));
        ar.SaveValue(30.f);
        ar.SaveValue(Uuid{});
        if (version >= 3) {
            ar.SaveValue(frames);
        }
        ar.SaveValue(1U);
        ar.SaveValue(channel.name);
        const auto write = [&ar](const auto &values) {
            const auto size = values.size() * sizeof(typename std::decay_t<decltype(values)>::value_type);
            ar.SaveValue(static_cast<uint32_t>(size));
            if (size != 0) {
                ar.SaveValue(reinterpret_cast<const char *>(values.data()), size);
            }
        };
        write(channel.position.times);
        write(channel.position.keys);
        write(channel.scale.times);
        write(channel.scale.keys);
        write(channel.rotation.times);
        write(channel.rotation.keys);
        return {bytes.Data(), bytes.Data() + bytes.Size()};
    }
} // namespace

TEST(AnimationAssetValidation, LegacyAndExplicitSourceDurationSurviveProductRoundTrip)
{
    for (const uint32_t version : {2U, 3U}) {
        const auto             bytes = Source(version, 90, ConstantChannel());
        IMemoryArchive         stream(bytes.data(), bytes.size());
        BinaryInputArchive     reader(stream);
        AnimationClipAssetData data;
        ASSERT_TRUE(data.LoadSource(reader));
        const uint32_t frames = version == 2 ? 61 : 90;
        EXPECT_EQ(data.sourceFrameCount, frames);
        for (auto &channel : data.nodeChannels) {
            channel.Compress();
        }
        auto bulk = data.BuildBulk();
        EXPECT_EQ(data.channels[0].positionTimes.size, sizeof(AnimTimeKey));
        OMemoryArchive      header;
        BinaryOutputArchive writer(header);
        data.Save(writer);
        IMemoryArchive     headerInput(header.Data(), header.Size());
        BinaryInputArchive headerReader(headerInput);
        auto               asset = std::make_shared<MemoryClipAsset>();
        asset->Data().Load(headerReader);
        ASSERT_TRUE(asset->Data().validation);
        asset->Bind(std::move(bulk));
        AnimationStatus status;
        const auto      clip = CreateAnimationClipFromAsset(asset, &status);
        ASSERT_TRUE(status) << status.message;
        ASSERT_NE(clip, nullptr);
        EXPECT_EQ(clip->GetFrameCount(), frames);
        EXPECT_FLOAT_EQ(clip->GetDuration(), static_cast<float>(frames) / 30.f);
    }
}

TEST(AnimationAssetValidation, RejectsInvalidSourceAndTruncatedArrays)
{
    auto                   channel  = ConstantChannel();
    const auto             tooShort = Source(3, 20, channel);
    IMemoryArchive         stream(tooShort.data(), tooShort.size());
    BinaryInputArchive     reader(stream);
    AnimationClipAssetData data;
    EXPECT_FALSE(data.LoadSource(reader));
    EXPECT_EQ(data.validation.error, AnimationError::InvalidDuration);

    channel.position.times[1]    = 0;
    const auto         duplicate = Source(2, 0, channel);
    IMemoryArchive     duplicateStream(duplicate.data(), duplicate.size());
    BinaryInputArchive duplicateReader(duplicateStream);
    EXPECT_FALSE(data.LoadSource(duplicateReader));
    EXPECT_EQ(data.validation.error, AnimationError::InvalidChannel);

    auto truncated = Source(2, 0, ConstantChannel());
    truncated.resize(truncated.size() - 1);
    IMemoryArchive     shortStream(truncated.data(), truncated.size());
    BinaryInputArchive shortReader(shortStream);
    EXPECT_FALSE(data.LoadSource(shortReader));
    EXPECT_EQ(data.validation.error, AnimationError::InvalidAsset);
}

TEST(AnimationAssetValidation, ObsoleteProductFailsHandlerWithRecookDiagnostic)
{
    OMemoryArchive      bytes;
    BinaryOutputArchive writer(bytes);
    writer.SaveValue(2U);
    IMemoryArchive              stream(bytes.Data(), bytes.Size());
    auto                        asset = std::make_shared<Asset<AnimationClip>>();
    AssetHandler<AnimationClip> handler;
    EXPECT_FALSE(handler.Load(stream, asset));
    EXPECT_EQ(asset->Data().validation.error, AnimationError::UnsupportedVersion);
    EXPECT_NE(asset->Data().validation.message.find("recook"), std::string::npos);
    AnimationStatus status;
    EXPECT_EQ(CreateAnimationClipFromAsset(asset, &status), nullptr);
    EXPECT_EQ(status.error, AnimationError::UnsupportedVersion);
}

TEST(AnimationAssetValidation, InvalidBulkFailsBeforeCopyOrLargeAllocation)
{
    auto  asset = std::make_shared<MemoryClipAsset>();
    auto &data  = asset->Data();
    data.nodeChannels.push_back(ConstantChannel());
    data.frameRate = 30.f;
    ASSERT_TRUE(data.PrepareSource());
    auto bulk = data.BuildBulk();
    asset->Bind(bulk);
    data.channels[0].positionKeys.size -= 1;
    AnimationStatus status;
    EXPECT_EQ(CreateAnimationClipFromAsset(asset, &status), nullptr);
    EXPECT_EQ(status.error, AnimationError::InvalidAsset);
    data.BuildBulk();
    data.channels[0].positionTimes.offset = UINT64_MAX;
    EXPECT_EQ(CreateAnimationClipFromAsset(asset, &status), nullptr);
    data.BuildBulk();
    bulk.pop_back();
    asset->Bind(bulk);
    EXPECT_EQ(CreateAnimationClipFromAsset(asset, &status), nullptr);
    EXPECT_TRUE(asset->ReadBulk(0, UINT32_MAX).empty());
}

TEST(AnimationAssetValidation, SourceWriterPreservesExplicitLength)
{
    AnimationClipAssetData data;
    data.name             = "idle";
    data.frameRate        = 30.f;
    data.sourceFrameCount = 120;
    data.nodeChannels.push_back(ConstantChannel());
    OMemoryArchive      bytes;
    BinaryOutputArchive writer(bytes);
    ASSERT_TRUE(data.SaveSource(writer));
    IMemoryArchive         stream(bytes.Data(), bytes.Size());
    BinaryInputArchive     reader(stream);
    AnimationClipAssetData loaded;
    ASSERT_TRUE(loaded.LoadSource(reader));
    EXPECT_EQ(loaded.sourceFrameCount, 120U);
    ASSERT_EQ(loaded.nodeChannels.size(), 1U);
    EXPECT_EQ(loaded.nodeChannels[0].position.times, data.nodeChannels[0].position.times);
}
