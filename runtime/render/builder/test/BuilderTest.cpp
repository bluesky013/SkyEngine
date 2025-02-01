//
// Created by blues on 2025/1/30.
//

#include <gtest/gtest.h>
#include <builder/render/image/ImageFilter.h>
#include <builder/render/image/ImageProcess.h>
#include <builder/render/image/ImageMipGen.h>
#include <builder/render/image/ImageConverter.h>
#include <core/math/Color.h>

using namespace sky::builder;

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
    ImageObjectPtr src = ImageObject::CreateImage2D(2, 2, PixelType::U8, 1);
    {
        ImageMipData mip0 = {2, 2, 1};
        mip0.data = std::make_unique<uint8_t[]>(sizeof(DFT_COLOR32));
        memcpy(mip0.data.get(), DFT_COLOR32, sizeof(DFT_COLOR32));
        src->mips.emplace_back(std::move(mip0));
    }


    ImageObjectPtr dst = ImageObject::CreateImage2D(2, 2, PixelType::Float, 4);
    {
        ImageMipData mip0 = {2, 2, 1};
        mip0.data = std::make_unique<uint8_t[]>(sizeof(DFT_COLOR));
        dst->mips.emplace_back(std::move(mip0));
    }

    ImageConverter converter(ImageConverter::Payload{src, dst});
    converter.DoWork();
}

TEST(BuilderTest, MipMapGenTest)
{
    ImageObjectPtr image = ImageObject::CreateImage2D(2, 2, PixelType::Float, 1);
    ImageMipData mip0 = {2, 2, 1};
    mip0.data = std::make_unique<uint8_t[]>(sizeof(DFT_COLOR));
    memcpy(mip0.data.get(), DFT_COLOR, sizeof(DFT_COLOR));
    image->mips.emplace_back(std::move(mip0));

    ImageMipGen mipGen(ImageMipGen::Payload{image});
    mipGen.DoWork();
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}