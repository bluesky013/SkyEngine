#include <content/builder/AnimationBuilder.h>

#include <animation/core/Skeleton.h>
#include <content/adaptor/ContentReflection.h>
#include <content/adaptor/assets/AnimationAsset.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

#include <filesystem>

using namespace sky;

namespace {
    class ClipProductFileSystem : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        bool                failCommit = false;
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (failCommit && to.FileName() == "current.bin") {
                return {FileOperationError::IOError, "injected clip publish failure"};
            }
            return NativeFileSystem::ReplaceFileAtomic(from, to);
        }
    };

    class FailedClipCompressor : public ICompressor {
    public:
        uint32_t CompressBound(uint32_t size) override
        {
            return size;
        }
        CompressResult Compress(const std::span<const uint8_t> &, const std::span<uint8_t> &, uint32_t) override
        {
            return {false, 0};
        }
        CompressResult DeCompress(const std::span<const uint8_t> &, const std::span<uint8_t> &, uint32_t) override
        {
            return {false, 0};
        }
    };
} // namespace

TEST(AnimationBuilderTest, RecookPublishesNewSnapshotAndFailuresKeepOldClip)
{
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-clip-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        CounterPtr<ClipProductFileSystem> filesystem = new ClipProductFileSystem(FilePath(root));
        auto                             *manager    = AssetManager::Get();
        manager->AddAssetProductBundle(new HashedAssetBundle(filesystem, "transactional-clip"));
        const auto        id = Uuid::Create();
        AssetBuildRequest request;
        request.bundle          = "transactional-clip";
        request.target          = request.bundle;
        request.assetInfo       = new AssetSourceInfo;
        request.assetInfo->uuid = id;
        request.assetInfo->path = "walk.clip";
        request.assetInfo->ext  = ".clip";
        request.file            = filesystem->CreateOrOpenFile("walk.clip");
        const auto writeSource  = [&](float translation) {
            AnimationClipAssetData source;
            source.name             = "walk";
            source.frameRate        = 30.f;
            source.sourceFrameCount = 61;
            AnimNodeChannelData channel;
            channel.name           = "root";
            channel.position.times = {0, 60};
            channel.position.keys  = {Vector3(translation, 0.f, 0.f), Vector3(translation, 0.f, 0.f)};
            source.nodeChannels.emplace_back(std::move(channel));
            const auto          output = request.file->WriteAsArchive();
            BinaryOutputArchive archive(*output);
            EXPECT_TRUE(source.SaveSource(archive));
            output->Flush();
        };
        builder::AnimationBuilder builder;
        AssetBuildResult          result{};
        writeSource(1.f);
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        const auto first = manager->LoadAsset<AnimationClip>(id);
        ASSERT_NE(first, nullptr);
        first->BlockUntilLoaded();
        ASSERT_TRUE(first->IsLoaded());
        ASSERT_NE(first->GetProductGroup(), nullptr);
        AnimationStatus status;
        const auto      firstClip = CreateAnimationClipFromAsset(first, &status);
        ASSERT_TRUE(status) << status.message;
        ASSERT_NE(firstClip, nullptr);
        EXPECT_EQ(firstClip->GetFrameCount(), 61U);
        writeSource(2.f);
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        const auto second = manager->LoadAsset<AnimationClip>(id);
        ASSERT_NE(second, nullptr);
        second->BlockUntilLoaded();
        ASSERT_TRUE(second->IsLoaded());
        EXPECT_NE(first.get(), second.get());
        EXPECT_EQ(second->GetProductGroup()->revision, first->GetProductGroup()->revision + 1);
        const auto oldClip = CreateAnimationClipFromAsset(first, &status);
        ASSERT_TRUE(status);
        const auto newClip = CreateAnimationClipFromAsset(second, &status);
        ASSERT_TRUE(status);
        SkeletonData rigData;
        rigData.boneData = {{Name("root"), INVALID_BONE_ID}};
        rigData.refPos.resize(1);
        const auto        rig  = Skeleton::BuildSkeleton(rigData);
        AnimPose          pose = *rig->GetRefPos();
        const SampleParam sample{{0, 0.f}, AnimInterpolation::LINEAR};
        ASSERT_TRUE(oldClip->SamplePose(pose, sample));
        EXPECT_FLOAT_EQ(pose.transforms.front().translation.x, 1.f);
        ASSERT_TRUE(newClip->SamplePose(pose, sample));
        EXPECT_FLOAT_EQ(pose.transforms.front().translation.x, 2.f);
        filesystem->failCommit = true;
        writeSource(3.f);
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_NE(result.error.find("publish"), std::string::npos);
        EXPECT_EQ(manager->LoadAsset<AnimationClip>(id).get(), second.get());
        filesystem->failCommit = false;
        CompressionManager::Get()->Register(CompressionMethod::LZ4, new FailedClipCompressor);
        manager->SetProductCompression(CompressionMethod::LZ4);
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_NE(result.error.find("compression"), std::string::npos);
        EXPECT_EQ(manager->LoadAsset<AnimationClip>(id).get(), second.get());
        manager->DisableProductCompression();
        CompressionManager::Get()->UnRegister(CompressionMethod::LZ4);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::ContentReflection(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}
