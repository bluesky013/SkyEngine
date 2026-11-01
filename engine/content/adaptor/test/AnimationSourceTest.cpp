#include <content/adaptor/AnimationClipSource.h>
#include <content/adaptor/ContentReflection.h>
#include <content/adaptor/assets/AnimationAsset.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(AnimationClipSource, PollOwnsExactSnapshotAfterSourceAndRequestAreReleased)
{
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-clip-source-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        NativeFileSystemPtr fs      = new NativeFileSystem(FilePath(root));
        auto               *manager = AssetManager::Get();
        manager->AddAssetProductBundle(new HashedAssetBundle(fs, "clip-source-test"));
        const auto id      = Uuid::Create();
        const auto product = std::static_pointer_cast<Asset<AnimationClip>>(manager->CreateBuildAsset(id, Name("AnimationClip")));
        ASSERT_NE(product, nullptr);
        auto &data            = product->Data();
        data.name             = "clip";
        data.frameRate        = 30;
        data.sourceFrameCount = 30;
        AnimNodeChannelData channel;
        channel.name           = "root";
        channel.position.times = {0};
        channel.position.keys  = {{1, 0, 0}};
        data.nodeChannels.push_back(channel);
        ASSERT_TRUE(data.PrepareSource());
        const auto bulk = data.BuildBulk();
        product->AddBulk(bulk.data(), bulk.size());
        ASSERT_TRUE(manager->SaveAsset(product, "clip-source-test"));
        auto source = content::CreateAnimationClipSource();
        ASSERT_NE(source, nullptr);
        auto request = source->Request(id);
        ASSERT_TRUE(request);
        EXPECT_NE(request.Poll().state, AnimationSourceState::Error);
        AssetExecutor::Get()->WaitForAll();
        auto ready = request.Poll();
        ASSERT_EQ(ready.state, AnimationSourceState::Ready) << ready.status.message;
        const auto loaded = manager->FindAsset<AnimationClip>(id);
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(ready.version.generation, loaded->GetInstanceId());
        EXPECT_EQ(ready.version.revision, loaded->GetRevision());
        ASSERT_NE(ready.resource, nullptr);
        EXPECT_FLOAT_EQ(ready.resource->GetDuration(), 1);
        EXPECT_EQ(source->Request({}).Poll().state, AnimationSourceState::Error);
        source.reset();
        request = {};
        EXPECT_TRUE(ready.assetLease);
        EXPECT_FLOAT_EQ(ready.resource->GetDuration(), 1);
        ready = {};
        EXPECT_EQ(AnimationSourceRequest<AnimationClip>{}.Poll().state, AnimationSourceState::Error);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}
