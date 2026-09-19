//
// ImageAssetData round-trip and version-guard tests (2D / 2D array / 3D / cube).
//

#include <aurora/adaptor/assets/ImageAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

using namespace sky;

namespace {

    void RoundTrip(const aurora::ImageAssetData &in, aurora::ImageAssetData &out)
    {
        OMemoryArchive     outArchive;
        BinaryOutputArchive writer(outArchive);
        in.Save(writer);

        IMemoryArchive    inArchive(outArchive.Data(), outArchive.Size());
        BinaryInputArchive reader(inArchive);
        out.Load(reader);
    }

    aurora::ImageAssetData MakeData(aurora::ImageAssetType type, uint32_t width, uint32_t height, uint32_t depth,
                                     uint32_t arrayLayers)
    {
        aurora::ImageAssetData data;
        data.format      = aurora::PixelFormat::RGBA8_UNORM;
        data.type        = type;
        data.width       = width;
        data.height      = height;
        data.depth       = depth;
        data.mipLevels   = 1;
        data.arrayLayers = arrayLayers;

        aurora::ImageSliceHeader slice;
        slice.offset = 0;
        slice.size   = 16;
        data.slices.push_back(slice);
        data.rawData.resize(16, 0xAB);
        return data;
    }

} // namespace

TEST(ImageAssetTest, RoundTrip2D)
{
    const auto in = MakeData(aurora::ImageAssetType::TEXTURE_2D, 4, 4, 1, 1);

    aurora::ImageAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.version, aurora::ImageAssetData::CURRENT_VERSION);
    EXPECT_EQ(out.type, aurora::ImageAssetType::TEXTURE_2D);
    EXPECT_EQ(out.format, aurora::PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(out.width, 4u);
    EXPECT_EQ(out.height, 4u);
    EXPECT_EQ(out.arrayLayers, 1u);
    ASSERT_EQ(out.slices.size(), 1u);
    EXPECT_EQ(out.slices[0].size, 16u);
    EXPECT_EQ(out.rawData, in.rawData);
}

TEST(ImageAssetTest, RoundTrip2DArray)
{
    const auto in = MakeData(aurora::ImageAssetType::TEXTURE_2D_ARRAY, 8, 8, 1, 4);

    aurora::ImageAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.type, aurora::ImageAssetType::TEXTURE_2D_ARRAY);
    EXPECT_EQ(out.arrayLayers, 4u);
    EXPECT_EQ(out.rawData, in.rawData);
}

TEST(ImageAssetTest, RoundTrip3D)
{
    const auto in = MakeData(aurora::ImageAssetType::TEXTURE_3D, 4, 4, 8, 1);

    aurora::ImageAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.type, aurora::ImageAssetType::TEXTURE_3D);
    EXPECT_EQ(out.depth, 8u);
    EXPECT_EQ(out.rawData, in.rawData);
}

TEST(ImageAssetTest, RoundTripCube)
{
    const auto in = MakeData(aurora::ImageAssetType::TEXTURE_CUBE, 16, 16, 1, 6);

    aurora::ImageAssetData out;
    RoundTrip(in, out);

    EXPECT_EQ(out.type, aurora::ImageAssetType::TEXTURE_CUBE);
    EXPECT_EQ(out.arrayLayers, 6u);
    EXPECT_EQ(out.rawData, in.rawData);
}

TEST(ImageAssetTest, VersionMismatchRejected)
{
    aurora::ImageAssetData in = MakeData(aurora::ImageAssetType::TEXTURE_2D, 4, 4, 1, 1);
    in.version                = aurora::ImageAssetData::CURRENT_VERSION + 1;

    aurora::ImageAssetData out;
    RoundTrip(in, out);

    // Load rejects a mismatched version and clears the payload.
    EXPECT_EQ(out.format, aurora::PixelFormat::UNDEFINED);
    EXPECT_TRUE(out.rawData.empty());
    EXPECT_TRUE(out.slices.empty());
}
