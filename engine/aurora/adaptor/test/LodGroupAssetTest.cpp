//
// LodGroupAssetData round-trip and version-guard tests.
//

#include <aurora/adaptor/assets/LodGroupAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

using namespace sky;

namespace {

    void RoundTrip(const aurora::LodGroupAssetData &in, aurora::LodGroupAssetData &out)
    {
        OMemoryArchive     outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive    inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

    aurora::LodGroupAssetData MakeData()
    {
        aurora::LodGroupAssetData data;
        data.levels.push_back(aurora::LodGroupLevelData{1.0f, Uuid::CreateWithSeed(1)});
        data.levels.push_back(aurora::LodGroupLevelData{0.5f, Uuid::CreateWithSeed(2)});
        data.levels.push_back(aurora::LodGroupLevelData{0.25f, Uuid::CreateWithSeed(3)});
        return data;
    }

} // namespace

TEST(LodGroupAssetTest, RoundTripLevelsAndVersion)
{
    const auto in = MakeData();

    aurora::LodGroupAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.version, aurora::LodGroupAssetData::CURRENT_VERSION);
    ASSERT_EQ(out.levels.size(), 3u);
    for (size_t i = 0; i < in.levels.size(); ++i) {
        EXPECT_FLOAT_EQ(out.levels[i].screenSize, in.levels[i].screenSize);
        EXPECT_EQ(out.levels[i].mesh, in.levels[i].mesh);
    }
}

TEST(LodGroupAssetTest, VersionMismatchRejected)
{
    aurora::LodGroupAssetData in = MakeData();
    in.version                   = aurora::LodGroupAssetData::CURRENT_VERSION + 1;

    aurora::LodGroupAssetData out;
    RoundTrip(in, out);

    // Load rejects a mismatched version and clears the payload.
    EXPECT_TRUE(out.levels.empty());
}
