#include <array>
#include <aurora/adaptor/AnimationSkeletonSource.h>
#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

TEST(AnimationSkeletonSource, PollPinsRigGenerationAndLayoutWithoutBlocking)
{
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-rig-source-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        NativeFileSystemPtr fs      = new NativeFileSystem(FilePath(root));
        auto               *manager = AssetManager::Get();
        manager->AddAssetProductBundle(new HashedAssetBundle(fs, "rig-source-test"));
        const auto id      = Uuid::Create();
        const auto product = std::static_pointer_cast<Asset<Skeleton>>(manager->CreateBuildAsset(id, Name("AuroraSkeleton")));
        ASSERT_NE(product, nullptr);
        product->Data().boneData = {{Name("root"), INVALID_BONE_ID}};
        product->Data().refPos.resize(1);
        ProductGroupTransaction transaction;
        ProductGroupSnapshotPtr group;
        ASSERT_TRUE(manager->BeginProductGroup(id, "rig-source-test", std::array<Uuid, 1>{id}, transaction));
        ASSERT_TRUE(manager->SaveProductGroup(transaction, "rig-source-test", std::array<AssetPtr, 1>{product}, group));
        auto                      code     = std::make_shared<int>(1);
        std::weak_ptr<const void> lifetime = code;
        auto                      source   = CreateAnimationSkeletonSource(code);
        ASSERT_NE(source, nullptr);
        auto request = source->Request(id);
        ASSERT_TRUE(request);
        EXPECT_NE(request.Poll().state, AnimationSourceState::Error);
        AssetExecutor::Get()->WaitForAll();
        auto ready = request.Poll();
        ASSERT_EQ(ready.state, AnimationSourceState::Ready) << ready.status.message;
        EXPECT_EQ(ready.version.generation, group->generation);
        EXPECT_EQ(ready.version.layout, ready.resource->GetLayoutSignature());
        EXPECT_EQ(source->Request({}).Poll().state, AnimationSourceState::Error);
        source.reset();
        code.reset();
        request = {};
        EXPECT_FALSE(lifetime.expired());
        ready = {};
        EXPECT_TRUE(lifetime.expired());
        EXPECT_EQ(CreateAnimationSkeletonSource({}), nullptr);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}
