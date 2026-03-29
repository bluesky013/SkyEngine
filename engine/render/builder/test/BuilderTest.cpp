//
// Created by blues on 2025/1/30.
//

#include <gtest/gtest.h>
#include <builder/render/image/ImageFilter.h>
#include <builder/render/image/ImageProcess.h>
#include <builder/render/image/ImageMipGen.h>
#include <builder/render/image/ImageConverter.h>
#include <builder/render/image/ImageResizer.h>
#include <core/math/Color.h>

using namespace sky;
using namespace sky::builder;
using namespace sky::rhi;

inline static float bessel0(float x)
{
    const float EPSILON = 1e-6f;
    float xh, sum, pow, ds;
    int k;

    xh = 0.5f * x;
    sum = 1.0f;
    pow = 1.0f;
    k = 0;
    ds = 1.0;
    while (ds > sum * EPSILON) {
        ++k;
        pow = pow * (xh / k);
        ds = pow * pow;
        sum = sum + ds;
    }

    return sum;
}

TEST(BuilderTest, FilterTestBessel0)
{
    EXPECT_FLOAT_EQ(filter::Bessel_i0(0.1f), bessel0(0.1f));
    EXPECT_FLOAT_EQ(filter::Bessel_i0(1.2f), bessel0(1.2f));
    EXPECT_FLOAT_EQ(filter::Bessel_i0(2.3f), bessel0(2.3f));
}

TEST(BuilderTest, MipMapTest)
{
    ASSERT_EQ(GetMipLevel(2, 1), 2);
    ASSERT_EQ(GetMipLevel(1024, 1), 11);
}

static const sky::Color DFT_COLOR[] = {
    sky::Color{1.0, 0.0, 0.0, 1.0},
    sky::Color{0.0, 1.0, 0.0, 1.0},
    sky::Color{0.0, 0.0, 1.0, 1.0},
    sky::Color{1.0, 1.0, 1.0, 1.0},
};

static const uint8_t DFT_COLOR32[] = {
    255, 0,   0,   255,
    0,   255, 0,   255,
    0,   0,   255, 255,
    255, 255, 255, 255
};

TEST(BuilderTest, ConvertTest)
{
    // src: 2x2 RGBA8
    ImageObjectPtr src = ImageObject::CreateImage2D(2, 2, PixelFormat::RGBA8_UNORM);
    src->FillMip0(DFT_COLOR32, sizeof(DFT_COLOR32));

    // dst: 2x2 RGBA32F
    ImageObjectPtr dst = ImageObject::CreateImage2D(2, 2, PixelFormat::RGBA32_SFLOAT);
    dst->FillMip0();

    ImageConverter converter(ImageConverter::Payload{src, dst, 1.f}); // gamma 1.0 = no correction
    converter.DoWork();

    // Verify: first pixel should be (1,0,0,1)
    const float *outPixels = reinterpret_cast<const float *>(dst->mips[0].data.get());
    EXPECT_FLOAT_EQ(outPixels[0], 1.0f); // R
    EXPECT_FLOAT_EQ(outPixels[1], 0.0f); // G
    EXPECT_FLOAT_EQ(outPixels[2], 0.0f); // B
    EXPECT_FLOAT_EQ(outPixels[3], 1.0f); // A

    // second pixel (0,1,0,1)
    EXPECT_FLOAT_EQ(outPixels[4], 0.0f);
    EXPECT_FLOAT_EQ(outPixels[5], 1.0f);
    EXPECT_FLOAT_EQ(outPixels[6], 0.0f);
    EXPECT_FLOAT_EQ(outPixels[7], 1.0f);
}

TEST(BuilderTest, MipMapGenTest)
{
    // 2x2 RGBA32F image
    ImageObjectPtr image = ImageObject::CreateImage2D(2, 2, PixelFormat::RGBA32_SFLOAT);
    image->FillMip0(reinterpret_cast<const uint8_t *>(DFT_COLOR), sizeof(DFT_COLOR));

    ImageMipGen mipGen(ImageMipGen::Payload{image});
    mipGen.DoWork();

    // 2x2 -> mip levels: 2 (2x2, 1x1)
    ASSERT_EQ(image->mips.size(), 2u);
    ASSERT_EQ(image->mips[1].width, 1u);
    ASSERT_EQ(image->mips[1].height, 1u);
}

// ---------------------------------------------------------------------------
// ImageResizer tests
// ---------------------------------------------------------------------------

TEST(BuilderTest, ResizerNoOpWhenWithinLimits)
{
    // 4x4 RGBA8 ¡ª max is larger, should be a no-op
    ImageObjectPtr image = ImageObject::CreateImage2D(4, 4, PixelFormat::RGBA8_UNORM);
    image->FillMip0();

    ImageResizer resizer(ImageResizer::Payload{image, 1024, 1024});
    resizer.DoWork();

    ASSERT_EQ(image->width, 4u);
    ASSERT_EQ(image->height, 4u);
}

TEST(BuilderTest, ResizerExactLimit)
{
    // Image exactly at the limit ¡ª no resize
    ImageObjectPtr image = ImageObject::CreateImage2D(512, 512, PixelFormat::RGBA8_UNORM);
    image->FillMip0();

    ImageResizer resizer(ImageResizer::Payload{image, 512, 512});
    resizer.DoWork();

    ASSERT_EQ(image->width, 512u);
    ASSERT_EQ(image->height, 512u);
}

TEST(BuilderTest, ResizerDownscaleSquare)
{
    // 8x8 -> max 4x4
    ImageObjectPtr image = ImageObject::CreateImage2D(8, 8, PixelFormat::RGBA8_UNORM);
    image->FillMip0();
    memset(image->mips[0].data.get(), 128, image->mips[0].dataLength);

    ImageResizer resizer(ImageResizer::Payload{image, 4, 4});
    resizer.DoWork();

    ASSERT_EQ(image->width, 4u);
    ASSERT_EQ(image->height, 4u);
    ASSERT_EQ(image->mips.size(), 1u);
    ASSERT_EQ(image->mips[0].dataLength, 4u * 4u * 4u); // 4x4 RGBA8 = 64 bytes
}

TEST(BuilderTest, ResizerDownscalePreservesAspectRatio)
{
    // 16x8 -> max 8x8 => should become 8x4 (scale by 0.5 to fit width)
    ImageObjectPtr image = ImageObject::CreateImage2D(16, 8, PixelFormat::RGBA8_UNORM);
    image->FillMip0();
    memset(image->mips[0].data.get(), 200, image->mips[0].dataLength);

    ImageResizer resizer(ImageResizer::Payload{image, 8, 8});
    resizer.DoWork();

    ASSERT_EQ(image->width, 8u);
    ASSERT_EQ(image->height, 4u);
}

TEST(BuilderTest, ResizerDownscalePreservesAspectRatioTall)
{
    // 8x16 -> max 8x8 => should become 4x8 (scale by 0.5 to fit height)
    ImageObjectPtr image = ImageObject::CreateImage2D(8, 16, PixelFormat::RGBA8_UNORM);
    image->FillMip0();
    memset(image->mips[0].data.get(), 100, image->mips[0].dataLength);

    ImageResizer resizer(ImageResizer::Payload{image, 8, 8});
    resizer.DoWork();

    ASSERT_EQ(image->width, 4u);
    ASSERT_EQ(image->height, 8u);
}

TEST(BuilderTest, ResizerWidthOnlyExceed)
{
    // 1024x64 -> max 256 wide, any height => 256x16
    ImageObjectPtr image = ImageObject::CreateImage2D(1024, 64, PixelFormat::RGBA8_UNORM);
    image->FillMip0();
    memset(image->mips[0].data.get(), 50, image->mips[0].dataLength);

    ImageResizer resizer(ImageResizer::Payload{image, 256, 0xFFFFFFFF});
    resizer.DoWork();

    ASSERT_EQ(image->width, 256u);
    ASSERT_EQ(image->height, 16u);
}

TEST(BuilderTest, ResizerFloat32Image)
{
    // Test with float format: 16x16 RGBA32F -> max 4x4
    ImageObjectPtr image = ImageObject::CreateImage2D(16, 16, PixelFormat::RGBA32_SFLOAT);
    image->FillMip0();

    // Fill with a known value
    float *pixels = reinterpret_cast<float *>(image->mips[0].data.get());
    uint32_t pixelCount = 16 * 16 * 4;
    for (uint32_t i = 0; i < pixelCount; ++i) {
        pixels[i] = 0.5f;
    }

    ImageResizer resizer(ImageResizer::Payload{image, 4, 4});
    resizer.DoWork();

    ASSERT_EQ(image->width, 4u);
    ASSERT_EQ(image->height, 4u);
    ASSERT_EQ(image->mips.size(), 1u);

    // A uniform 0.5 image downscaled should still be ~0.5
    const float *outPixels = reinterpret_cast<const float *>(image->mips[0].data.get());
    for (uint32_t i = 0; i < 4u * 4u * 4u; ++i) {
        EXPECT_NEAR(outPixels[i], 0.5f, 0.05f);
    }
}

TEST(BuilderTest, ResizerDiscardsMipChain)
{
    // Generate mips first, then resize ¡ª should discard mip chain
    ImageObjectPtr image = ImageObject::CreateImage2D(8, 8, PixelFormat::RGBA8_UNORM);
    image->FillMip0();
    memset(image->mips[0].data.get(), 128, image->mips[0].dataLength);

    ImageMipGen mipGen(ImageMipGen::Payload{image});
    mipGen.DoWork();
    ASSERT_GT(image->mips.size(), 1u);

    ImageResizer resizer(ImageResizer::Payload{image, 4, 4});
    resizer.DoWork();

    ASSERT_EQ(image->width, 4u);
    ASSERT_EQ(image->height, 4u);
    ASSERT_EQ(image->mips.size(), 1u); // mip chain discarded
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}