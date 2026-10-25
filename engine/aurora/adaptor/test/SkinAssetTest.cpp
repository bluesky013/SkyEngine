#include <animation/core/Skeleton.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <core/archive/MemoryStreamArchive.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

TEST(SkinAsset, BindingRoundTripAndSkeletonBridge)
{
    SkinAssetData source;
    source.sourceOwner   = Uuid::CreateWithSeed(42);
    source.binding.input = {77, {"slot0"}};
    source.binding.binds = {{Matrix4::Identity(), "root", 0}};
    OMemoryArchive      bytes;
    BinaryOutputArchive writer(bytes);
    source.Save(writer);
    ASSERT_TRUE(writer.IsValid());
    IMemoryArchive     stream(bytes.Data(), bytes.Size());
    BinaryInputArchive reader(stream);
    auto               asset = std::make_shared<Asset<Skin>>();
    asset->Data().Load(reader);
    ASSERT_TRUE(asset->Data().IsLoadValid());
    EXPECT_EQ(asset->Data().sourceOwner, source.sourceOwner);
    SkinStatus status;
    const auto skin = FetchOrCreateSkinByAsset(asset, &status);
    ASSERT_TRUE(status);
    ASSERT_NE(skin, nullptr);
    EXPECT_EQ(skin.Get(), FetchOrCreateSkinByAsset(asset).Get());

    SkeletonData rig;
    rig.boneData = {{Name("root"), INVALID_BONE_ID}};
    rig.refPos.resize(1);
    const auto       skeleton = Skeleton::BuildSkeleton(rig);
    SkinTargetLayout target;
    ASSERT_TRUE(MakeSkinTargetLayout(*skeleton, target));
    const SkinBindingKey   key{Uuid::CreateWithSeed(1), Uuid::CreateWithSeed(2), Uuid::CreateWithSeed(3), 1, 1, 1, target.signature,
                             Uuid::CreateWithSeed(4), Uuid::CreateWithSeed(5), Uuid::CreateWithSeed(6)};
    ResolvedSkinBindingPtr binding;
    ASSERT_TRUE(ResolveSkinBinding(source.binding.input, *skin, target, key, binding));
    EXPECT_EQ(binding->slotToBone[0], 0);
}

TEST(SkinAsset, RejectsLegacyAndTruncatedProducts)
{
    OMemoryArchive      legacy;
    BinaryOutputArchive writer(legacy);
    writer.SaveValue(1U);
    IMemoryArchive     stream(legacy.Data(), legacy.Size());
    BinaryInputArchive reader(stream);
    auto               asset = std::make_shared<Asset<Skin>>();
    asset->Data().Load(reader);
    EXPECT_FALSE(asset->Data().IsLoadValid());
    EXPECT_EQ(asset->Data().validation.error, SkinError::UnsupportedVersion);
    EXPECT_EQ(FetchOrCreateSkinByAsset(asset), nullptr);
    SkinAssetData source;
    source.binding.input = {77, {"slot0"}};
    source.binding.binds = {{Matrix4::Identity(), "root", 0}};
    OMemoryArchive      bytes;
    BinaryOutputArchive currentWriter(bytes);
    source.Save(currentWriter);
    IMemoryArchive     truncated(bytes.Data(), bytes.Size() - 1);
    BinaryInputArchive currentReader(truncated);
    asset->Data().Load(currentReader);
    EXPECT_FALSE(asset->Data().IsLoadValid());
    EXPECT_EQ(asset->Data().validation.error, SkinError::InvalidData);
    EXPECT_EQ(FetchOrCreateSkinByAsset(asset), nullptr);
}
