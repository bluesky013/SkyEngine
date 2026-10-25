#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <core/archive/MemoryStreamArchive.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

TEST(SkeletonAsset, ValidatesHierarchyAndRuntimeSignatureOnLoad)
{
    SkeletonAssetData data;
    data.boneData = {{Name("root"), INVALID_BONE_ID}, {Name("root/helper"), 0}};
    data.refPos.resize(2);
    OMemoryArchive      bytes;
    BinaryOutputArchive writer(bytes);
    data.Save(writer);
    IMemoryArchive     stream(bytes.Data(), bytes.Size());
    BinaryInputArchive reader(stream);
    auto               asset = std::make_shared<Asset<Skeleton>>();
    asset->Data().Load(reader);
    ASSERT_TRUE(asset->Data().IsLoadValid());
    AnimationStatus status;
    const auto      skeleton = FetchOrCreateSkeletonByAsset(asset, &status);
    ASSERT_TRUE(status);
    ASSERT_NE(skeleton, nullptr);
    EXPECT_EQ(skeleton->GetLayoutSignature(), Skeleton::BuildSkeleton(data)->GetLayoutSignature());

    data.boneData[0].parentIndex = 1;
    OMemoryArchive      badBytes;
    BinaryOutputArchive badWriter(badBytes);
    data.Save(badWriter);
    IMemoryArchive     badStream(badBytes.Data(), badBytes.Size());
    BinaryInputArchive badReader(badStream);
    asset = std::make_shared<Asset<Skeleton>>();
    asset->Data().Load(badReader);
    EXPECT_FALSE(asset->Data().IsLoadValid());
    EXPECT_EQ(FetchOrCreateSkeletonByAsset(asset, &status), nullptr);
    EXPECT_EQ(status.error, AnimationError::InvalidSkeleton);
}

TEST(SkeletonAsset, RejectsOversizedCountAndTruncatedTransforms)
{
    OMemoryArchive      bytes;
    BinaryOutputArchive writer(bytes);
    writer.SaveValue(SkeletonAssetData::CURRENT_VERSION);
    writer.SaveValue(UINT32_MAX);
    IMemoryArchive     stream(bytes.Data(), bytes.Size());
    BinaryInputArchive reader(stream);
    SkeletonAssetData  data;
    data.Load(reader);
    EXPECT_FALSE(data.IsLoadValid());
    EXPECT_TRUE(data.boneData.empty());

    data.clear();
    data.boneData = {{Name("root"), INVALID_BONE_ID}};
    data.refPos.resize(1);
    OMemoryArchive      shortBytes;
    BinaryOutputArchive shortWriter(shortBytes);
    data.Save(shortWriter);
    IMemoryArchive     shortStream(shortBytes.Data(), shortBytes.Size() - 1);
    BinaryInputArchive shortReader(shortStream);
    data.Load(shortReader);
    EXPECT_FALSE(data.IsLoadValid());
}
