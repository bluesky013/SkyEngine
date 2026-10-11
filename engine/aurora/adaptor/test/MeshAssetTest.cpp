//
// MeshAssetData round-trip, material slot resolution and version-guard tests.
//

#include <aurora/adaptor/assets/MeshAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

using namespace sky;

namespace {

    void RoundTrip(const aurora::MeshAssetData &in, aurora::MeshAssetData &out)
    {
        OMemoryArchive      outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive     inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

    aurora::MeshAssetData MakeData()
    {
        aurora::MeshAssetData data;
        data.vertexOffset = 0;
        data.vertexSize   = 4;
        data.vertexStride = 16;
        data.vertexCount  = 1;
        data.indexOffset  = 4;
        data.indexSize    = 6;
        data.indexCount   = 3; // 2 x uint16 indices

        aurora::MeshSubMeshData first;
        first.indexOffset   = 0;
        first.indexCount    = 3;
        first.materialIndex = 0;
        data.subMeshes.push_back(first);

        aurora::MeshSubMeshData second;
        second.indexOffset   = 3;
        second.indexCount    = 3;
        second.materialIndex = 1;
        data.subMeshes.push_back(second);

        data.materials.push_back(Uuid::CreateWithSeed(1));
        data.materials.push_back(Uuid::CreateWithSeed(2));
        return data;
    }

} // namespace

TEST(MeshAssetTest, RoundTripMaterialsAndVersion)
{
    const auto in = MakeData();

    aurora::MeshAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.version, aurora::MeshAssetData::CURRENT_VERSION);
    EXPECT_EQ(out.vertexOffset, in.vertexOffset);
    EXPECT_EQ(out.vertexSize, in.vertexSize);
    EXPECT_EQ(out.indexOffset, in.indexOffset);
    EXPECT_EQ(out.indexSize, in.indexSize);
    ASSERT_EQ(out.subMeshes.size(), 2u);
    EXPECT_EQ(out.subMeshes[1].materialIndex, 1u);
    ASSERT_EQ(out.materials.size(), 2u);
    EXPECT_EQ(out.materials[0], in.materials[0]);
    EXPECT_EQ(out.materials[1], in.materials[1]);
}

TEST(MeshAssetTest, GetMaterialUuidHit)
{
    const auto data = MakeData();

    ASSERT_NE(data.GetMaterialUuid(1), nullptr);
    EXPECT_EQ(*data.GetMaterialUuid(1), data.materials[1]);
}

TEST(MeshAssetTest, GetMaterialUuidEmptyTable)
{
    aurora::MeshAssetData data;

    EXPECT_EQ(data.GetMaterialUuid(0), nullptr);
}

TEST(MeshAssetTest, GetMaterialUuidOutOfRangeFallsBackToSlotZero)
{
    const auto data = MakeData();

    ASSERT_NE(data.GetMaterialUuid(7), nullptr);
    EXPECT_EQ(*data.GetMaterialUuid(7), data.materials[0]);
}

TEST(MeshAssetTest, VersionMismatchRejected)
{
    aurora::MeshAssetData in = MakeData();
    in.version               = aurora::MeshAssetData::CURRENT_VERSION + 1;

    aurora::MeshAssetData out;
    RoundTrip(in, out);

    // Load rejects a mismatched version and clears the payload.
    EXPECT_EQ(out.vertexSize, 0u);
    EXPECT_EQ(out.indexSize, 0u);
    EXPECT_TRUE(out.subMeshes.empty());
    EXPECT_TRUE(out.materials.empty());
}
