//
// Image cook pipeline tests: mip chain, resize-to-limit and gamma conversion.
//

#include <aurora/cook/image/ImageConverter.h>
#include <aurora/cook/image/ImageFilter.h>
#include <aurora/cook/image/ImageMipGen.h>
#include <aurora/cook/image/ImageResizer.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::cook;

namespace {

    ImageObjectPtr MakeRGBA8(uint32_t width, uint32_t height)
    {
        auto image = ImageObject::CreateImage2D(width, height, PixelFormat::RGBA8_UNORM);
        image->FillMip0();
        return image;
    }

    void FillSolid(ImageObject &image, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t mip = 0)
    {
        auto &data = image.mips[mip];
        for (uint32_t i = 0; i < data.width * data.height; ++i) {
            data.data[i * 4 + 0] = r;
            data.data[i * 4 + 1] = g;
            data.data[i * 4 + 2] = b;
            data.data[i * 4 + 3] = a;
        }
    }

} // namespace

TEST(ImageProcessTest, GetMipLevel)
{
    EXPECT_EQ(GetMipLevel(1, 1), 1u);
    EXPECT_EQ(GetMipLevel(4, 4), 3u);
    EXPECT_EQ(GetMipLevel(8, 4), 4u);
}

TEST(ImageProcessTest, FormatHelpers)
{
    EXPECT_EQ(GetNumComp(PixelFormat::RGBA8_UNORM), 4u);
    EXPECT_EQ(GetBytePerComp(PixelFormat::RGBA8_UNORM), 1u);

    EXPECT_EQ(GetNumComp(PixelFormat::R16_UNORM), 1u);
    EXPECT_EQ(GetBytePerComp(PixelFormat::R16_UNORM), 2u);

    EXPECT_EQ(GetNumComp(PixelFormat::RGBA32_SFLOAT), 4u);
    EXPECT_EQ(GetBytePerComp(PixelFormat::RGBA32_SFLOAT), 4u);

    // Block-compressed formats have no meaningful per-component byte size.
    EXPECT_EQ(GetBytePerComp(PixelFormat::BC7_UNORM_BLOCK), 0u);

    EXPECT_EQ(GetPixelType(PixelFormat::RGBA8_SRGB), PixelType::U8);
    EXPECT_EQ(GetPixelType(PixelFormat::RGBA16_SFLOAT), PixelType::HALF);
    EXPECT_EQ(GetPixelType(PixelFormat::RGBA32_SFLOAT), PixelType::Float);
}

TEST(ImageProcessTest, ColorRoundTripU8)
{
    const uint8_t src[4] = {10, 20, 30, 40};
    Color         color;
    GetImageColor(PixelType::U8, 4, src, color);
    EXPECT_NEAR(color.v[0], 10.f / 255.f, 1e-5f);
    EXPECT_NEAR(color.v[3], 40.f / 255.f, 1e-5f);

    uint8_t dst[4] = {};
    SetImageColor(PixelType::U8, 4, dst, color);
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
    EXPECT_EQ(dst[3], 40);
}

TEST(ImageProcessTest, ColorRoundTripFloat)
{
    const float src[4] = {0.25f, -1.5f, 2.0f, 0.5f};
    Color       color;
    GetImageColor(PixelType::Float, 4, reinterpret_cast<const uint8_t *>(src), color);
    EXPECT_FLOAT_EQ(color.v[1], -1.5f);

    float dst[4] = {};
    SetImageColor(PixelType::Float, 4, reinterpret_cast<uint8_t *>(dst), color);
    EXPECT_FLOAT_EQ(dst[0], 0.25f);
    EXPECT_FLOAT_EQ(dst[2], 2.0f);
}

TEST(ImageProcessTest, ColorHalfKnownBits)
{
    // 0x3C00 -> 1.0f, 0x3800 -> 0.5f (IEEE 754 binary16).
    uint16_t src = 0x3C00;
    Color    color;
    GetImageColor(PixelType::HALF, 1, reinterpret_cast<const uint8_t *>(&src), color);
    EXPECT_FLOAT_EQ(color.v[0], 1.0f);

    color.v[0] = 0.5f;
    uint16_t dst = 0;
    SetImageColor(PixelType::HALF, 1, reinterpret_cast<uint8_t *>(&dst), color);
    EXPECT_EQ(dst, 0x3800);
}

TEST(ImageProcessTest, CreateFromImageCopiesLayout)
{
    auto src = MakeRGBA8(4, 2);
    FillSolid(*src, 1, 2, 3, 4);

    auto copy = ImageObject::CreateFromImage(src);
    EXPECT_EQ(copy->width, 4u);
    EXPECT_EQ(copy->height, 2u);
    EXPECT_EQ(copy->components, src->components);
    EXPECT_EQ(copy->pixelSize, src->pixelSize);
    ASSERT_EQ(copy->mips.size(), src->mips.size());
    EXPECT_NE(copy->mips[0].data, nullptr);
}

TEST(ImageProcessTest, CreateCubeHasSixLayers)
{
    auto cube = ImageObject::CreateImageCube(2, 2, PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(cube->depth, 6u);
    EXPECT_EQ(cube->width, 2u);
}

TEST(ImageProcessTest, CompressedImageKeepsMipLayout)
{
    auto image = MakeRGBA8(4, 4);
    image->mips.emplace_back(ImageMipData::Create(2, 2, 1, image->pixelSize));

    auto compressed = CompressedImage::CreateFromImageObject(image, PixelFormat::BC7_UNORM_BLOCK);
    EXPECT_EQ(compressed->format, PixelFormat::BC7_UNORM_BLOCK);
    ASSERT_EQ(compressed->mips.size(), 2u);
    EXPECT_EQ(compressed->mips[1].width, 2u);
}

TEST(ImageProcessTest, MipChainLevelsAndSizes)
{
    auto image = MakeRGBA8(8, 8);
    FillSolid(*image, 200, 100, 50, 255);

    ImageMipGen::Payload payload;
    payload.image = image;
    payload.type  = MipGenType::Kaiser;
    ImageMipGen gen(payload);
    gen.DoWork();

    EXPECT_EQ(image->mips.size(), 4u); // 8, 4, 2, 1
    EXPECT_EQ(image->mips[1].width, 4u);
    EXPECT_EQ(image->mips[2].width, 2u);
    EXPECT_EQ(image->mips[3].width, 1u);

    // Filtering a solid color must keep that color at every level.
    EXPECT_NEAR(image->mips[2].data[0], 200, 2);
    EXPECT_NEAR(image->mips[2].data[1], 100, 2);
    EXPECT_NEAR(image->mips[2].data[2], 50, 2);
}

TEST(ImageProcessTest, MipChainBoxFilter)
{
    auto image = MakeRGBA8(4, 4);
    FillSolid(*image, 60, 120, 180, 255);

    ImageMipGen::Payload payload;
    payload.image = image;
    payload.type  = MipGenType::Box;
    ImageMipGen gen(payload);
    gen.DoWork();

    EXPECT_EQ(image->mips.size(), 3u); // 4, 2, 1
    EXPECT_NEAR(image->mips[1].data[0], 60, 2);
    EXPECT_NEAR(image->mips[1].data[2], 180, 2);
}

TEST(ImageProcessTest, Lanczos3KernelValues)
{
    LanczosFilter<float>                 lanczos(3.f);
    sky::aurora::cook::Filter<float>    &kernel = lanczos;

    EXPECT_NEAR(kernel.Eval(0.f), 1.f, 1e-4f);  // sinc(0)^2
    EXPECT_NEAR(kernel.Eval(1.f), 0.f, 1e-4f);  // integer sinc zeros
    EXPECT_NEAR(kernel.Eval(3.f), 0.f, 1e-4f);  // support edge
    EXPECT_NEAR(kernel.Eval(4.f), 0.f, 1e-4f);  // outside support
}

TEST(ImageProcessTest, MipChainLanczos3)
{
    auto image = MakeRGBA8(4, 4);
    FillSolid(*image, 60, 120, 180, 255);

    ImageMipGen::Payload payload;
    payload.image = image;
    payload.type  = MipGenType::Lanczos3;
    ImageMipGen gen(payload);
    gen.DoWork();

    EXPECT_EQ(image->mips.size(), 3u); // 4, 2, 1
    EXPECT_NEAR(image->mips[1].data[0], 60, 2);
    EXPECT_NEAR(image->mips[1].data[2], 180, 2);
}

TEST(ImageProcessTest, BoxDownscaleAlignsTaps)
{
    // Exact 2x box downscale of [0,1,2,3] must average pairs -> [0.5, 2.5].
    // A half-pixel tap offset would produce [1.5, ...] instead.
    auto image = ImageObject::CreateImage2D(4, 1, PixelFormat::RGBA32_SFLOAT);
    image->FillMip0();

    auto *px = reinterpret_cast<float *>(image->mips[0].data.get());
    for (uint32_t x = 0; x < 4; ++x) {
        px[x * 4 + 0] = static_cast<float>(x);
        px[x * 4 + 1] = 0.f;
        px[x * 4 + 2] = 0.f;
        px[x * 4 + 3] = 1.f;
    }

    ImageMipGen::Payload payload;
    payload.image = image;
    payload.type  = MipGenType::Box;
    ImageMipGen gen(payload);
    gen.DoWork();

    ASSERT_GE(image->mips.size(), 2u); // 4x1 -> 2x1 -> 1x1
    auto *out = reinterpret_cast<const float *>(image->mips[1].data.get());
    EXPECT_NEAR(out[0], 0.5f, 1e-3f);
    EXPECT_NEAR(out[1 * 4 + 0], 2.5f, 1e-3f);
}

TEST(ImageProcessTest, MipChainHalfSource)
{
    auto image = ImageObject::CreateImage2D(2, 2, PixelFormat::RGBA16_SFLOAT);
    image->FillMip0();

    // Half 1.0 (0x3C00) in every channel; filtering must not collapse to 0.
    auto &src = image->mips[0];
    auto *half = reinterpret_cast<uint16_t *>(src.data.get());
    for (uint32_t i = 0; i < src.width * src.height * 4; ++i) {
        half[i] = 0x3C00;
    }

    ImageMipGen::Payload payload;
    payload.image = image;
    payload.type  = MipGenType::Box;
    ImageMipGen gen(payload);
    gen.DoWork();

    ASSERT_EQ(image->mips.size(), 2u); // 2, 1
    auto *out = reinterpret_cast<const uint16_t *>(image->mips[1].data.get());
    EXPECT_NEAR(HalfToFloat(out[0]), 1.0f, 1e-3f);
    EXPECT_NEAR(HalfToFloat(out[1]), 1.0f, 1e-3f);
}

TEST(ImageProcessTest, ResizeToLimit)
{
    auto image = MakeRGBA8(8, 4);
    FillSolid(*image, 10, 20, 30, 255);

    ImageResizer::Payload payload;
    payload.image     = image;
    payload.maxWidth  = 4;
    payload.maxHeight = 4;
    ImageResizer resizer(payload);
    resizer.DoWork();

    EXPECT_EQ(image->width, 4u);
    EXPECT_EQ(image->height, 2u);
    ASSERT_EQ(image->mips.size(), 1u);
    EXPECT_EQ(image->mips[0].width, 4u);
}

TEST(ImageProcessTest, ResizeNoopWithinLimit)
{
    auto image = MakeRGBA8(4, 4);

    ImageResizer::Payload payload;
    payload.image     = image;
    payload.maxWidth  = 8;
    payload.maxHeight = 8;
    ImageResizer resizer(payload);
    resizer.DoWork();

    EXPECT_EQ(image->width, 4u);
    EXPECT_EQ(image->height, 4u);
}

TEST(ImageProcessTest, GammaConversion)
{
    auto src = MakeRGBA8(1, 1);
    auto dst = MakeRGBA8(1, 1);
    FillSolid(*src, 128, 128, 128, 255);

    ImageConverter::Payload payload;
    payload.src   = src;
    payload.dst   = dst;
    payload.gamma = 2.0f;
    ImageConverter converter(payload);
    converter.DoWork();

    // 128/255 = 0.50196; ^2 = 0.25196; *255 rounded = 64.
    EXPECT_EQ(dst->mips[0].data[0], 64);
}

TEST(ImageProcessTest, GammaIdentityKeepsPixels)
{
    auto src = MakeRGBA8(1, 1);
    auto dst = MakeRGBA8(1, 1);
    FillSolid(*src, 77, 88, 99, 255);

    ImageConverter::Payload payload;
    payload.src   = src;
    payload.dst   = dst;
    payload.gamma = 1.0f;
    ImageConverter converter(payload);
    converter.DoWork();

    EXPECT_EQ(dst->mips[0].data[0], 77);
    EXPECT_EQ(dst->mips[0].data[1], 88);
    EXPECT_EQ(dst->mips[0].data[2], 99);
}
