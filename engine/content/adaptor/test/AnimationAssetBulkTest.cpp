//
// AnimationClip header + bulk round-trip: the channel table loads without the
// times/keys, and CreateAnimationClipFromAsset reads them from the bulk on demand.
//

#include <content/adaptor/ContentReflection.h>
#include <content/adaptor/assets/AnimationAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <core/math/Vector3.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace sky;

namespace {

    // Serializes the authoring (.clip) source: one "root" channel with a 2-key position track.
    std::vector<uint8_t> MakeClipSource()
    {
        const std::vector<AnimTimeKey> positionTimes = {0, 10};
        const std::vector<Vector3>     positionKeys  = {Vector3{0.f, 0.f, 0.f}, Vector3{1.f, 1.f, 1.f}};

        OMemoryArchive      out;
        BinaryOutputArchive ar(out);

        ar.SaveValue(AnimationClipAssetData::CURRENT_VERSION);
        ar.SaveValue(std::string("walk"));
        ar.SaveValue(1.f);
        ar.SaveValue(Uuid{});
        ar.SaveValue(static_cast<uint32_t>(1));
        ar.SaveValue(std::string("root"));

        ar.SaveValue(static_cast<uint32_t>(positionTimes.size() * sizeof(AnimTimeKey)));
        ar.SaveValue(reinterpret_cast<const char *>(positionTimes.data()), positionTimes.size() * sizeof(AnimTimeKey));
        ar.SaveValue(static_cast<uint32_t>(positionKeys.size() * sizeof(Vector3)));
        ar.SaveValue(reinterpret_cast<const char *>(positionKeys.data()), positionKeys.size() * sizeof(Vector3));

        ar.SaveValue(static_cast<uint32_t>(0)); // scale times
        ar.SaveValue(static_cast<uint32_t>(0)); // scale keys
        ar.SaveValue(static_cast<uint32_t>(0)); // rotation times
        ar.SaveValue(static_cast<uint32_t>(0)); // rotation keys

        return std::vector<uint8_t>(out.Data(), out.Data() + out.Size());
    }

} // namespace

TEST(AnimationAssetBulkTest, ClipBulkRoundTrip)
{
    ContentReflection(SerializationContext::Get());

    const auto bundleRoot = std::filesystem::temp_directory_path() / "sky_content_clip_bulk";
    std::filesystem::remove_all(bundleRoot);
    NativeFileSystemPtr bundleFs = new NativeFileSystem(FilePath((bundleRoot / "b").string()));
    AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, "clipbulk"));

    const std::vector<uint8_t> source = MakeClipSource();
    const Uuid                 id     = Uuid::Create();

    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<AnimationClip>(id);
        ASSERT_NE(asset, nullptr);

        IMemoryArchive     in(source.data(), source.size());
        BinaryInputArchive archive(in);
        auto              &data = asset->Data();
        data.LoadSource(archive);
        ASSERT_EQ(data.version, AnimationClipAssetData::CURRENT_VERSION);
        ASSERT_EQ(data.nodeChannels.size(), 1u);

        auto bulk = data.BuildBulk();
        ASSERT_EQ(data.channels.size(), 1u);
        EXPECT_EQ(data.channels[0].name, "root");
        EXPECT_EQ(data.channels[0].positionTimes.size, 2u * sizeof(AnimTimeKey));
        EXPECT_EQ(data.channels[0].positionKeys.size, 2u * sizeof(Vector3));

        asset->AddBulk(bulk.data(), bulk.size());
        AssetManager::Get()->SaveAsset(asset, "clipbulk");
    }

    auto loaded = AssetManager::Get()->LoadAsset<AnimationClip>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());

    const auto &header = loaded->Data();
    ASSERT_EQ(header.channels.size(), 1u);
    EXPECT_EQ(header.channels[0].name, "root");
    EXPECT_TRUE(loaded->HasBulk());

    auto clip = CreateAnimationClipFromAsset(loaded);
    ASSERT_NE(clip, nullptr);
    EXPECT_FLOAT_EQ(clip->GetPlayRate(), 1.f);
    EXPECT_EQ(clip->GetFrameCount(), 11u); // last key time (10) + 1, read from the bulk

    loaded.reset();
    std::filesystem::remove_all(bundleRoot);
}
