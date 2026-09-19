//
// Cook pipeline tests: KTX parsing, block compression sizing, ImageAssetData
// output mapping and build-preset bundle resolution.
//

#include <aurora/cook/image/ImageAssetWriter.h>
#include <aurora/cook/image/ImageBuildConfig.h>
#include <aurora/cook/image/ImageCompressor.h>
#include <aurora/cook/image/ImageSource.h>

#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/JsonArchive.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::cook;

namespace {

    void Put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
    {
        std::memcpy(bytes.data() + offset, &value, 4);
    }

    void Put64(std::vector<uint8_t> &bytes, size_t offset, uint64_t value)
    {
        std::memcpy(bytes.data() + offset, &value, 8);
    }

    const uint8_t KTX1_ID[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    const uint8_t KTX2_ID[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> MakeKtx1()
    {
        std::vector<uint8_t> bytes(64 + 4 + 16, 0);
        std::memcpy(bytes.data(), KTX1_ID, 12);
        Put32(bytes, 12, 0x04030201);
        Put32(bytes, 16, 0x1401); // GL_UNSIGNED_BYTE
        Put32(bytes, 20, 1);
        Put32(bytes, 24, 0x1908); // GL_RGBA
        Put32(bytes, 28, 0x8058); // GL_RGBA8
        Put32(bytes, 32, 0x1908);
        Put32(bytes, 36, 2);
        Put32(bytes, 40, 2);
        Put32(bytes, 52, 1); // faces
        Put32(bytes, 56, 1); // mips
        Put32(bytes, 64, 16);
        for (uint32_t i = 0; i < 16; ++i) {
            bytes[68 + i] = static_cast<uint8_t>(i * 10);
        }
        return bytes;
    }

    std::vector<uint8_t> MakeKtx2(uint32_t superCompression)
    {
        std::vector<uint8_t> bytes(80 + 24 + 16, 0);
        std::memcpy(bytes.data(), KTX2_ID, 12);
        Put32(bytes, 12, 37); // VK_FORMAT_R8G8B8A8_UNORM
        Put32(bytes, 16, 1);
        Put32(bytes, 20, 2);
        Put32(bytes, 24, 2);
        Put32(bytes, 28, 1);
        Put32(bytes, 32, 1); // layerCount
        Put32(bytes, 36, 1); // faceCount
        Put32(bytes, 40, 1); // levelCount
        Put32(bytes, 44, superCompression);

        Put64(bytes, 80, 104); // level 0 byteOffset
        Put64(bytes, 88, 16);  // level 0 byteLength
        for (uint32_t i = 0; i < 16; ++i) {
            bytes[104 + i] = static_cast<uint8_t>(i);
        }
        return bytes;
    }

    ImageObjectPtr MakeRGBA8(uint32_t width, uint32_t height, uint32_t depth = 1)
    {
        auto image = ImageObject::CreateImage2D(width, height, PixelFormat::RGBA8_UNORM);
        image->depth = depth;
        image->FillMip0();
        for (uint32_t i = 0; i < width * height * depth; ++i) {
            image->mips[0].data[i * 4 + 0] = 10;
            image->mips[0].data[i * 4 + 1] = 20;
            image->mips[0].data[i * 4 + 2] = 30;
            image->mips[0].data[i * 4 + 3] = 255;
        }
        return image;
    }

    uint32_t CompressOne(const PixelFormat format, const ImageBuildConfig &config, uint32_t width, uint32_t height)
    {
        auto image = MakeRGBA8(width, height);
        auto compressed = CompressedImage::CreateFromImageObject(image, format);

        ImageCompressor::Payload payload;
        payload.image      = image;
        payload.compressed = compressed;
        payload.config     = config;
        payload.mip        = 0;
        payload.hasAlpha   = false;
        ImageCompressor(payload).DoWork();

        EXPECT_EQ(compressed->format, format);
        return compressed->mips[0].dataLength;
    }

} // namespace

TEST(CookSourceTest, ParseKtx1)
{
    CookImageSource source;
    ASSERT_TRUE(LoadKtx(MakeKtx1(), source));
    ASSERT_NE(source.image, nullptr);
    EXPECT_EQ(source.image->format, PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(source.image->width, 2u);
    EXPECT_EQ(source.image->mips[0].data[0], 0u);
    EXPECT_EQ(source.image->mips[0].data[1], 10u);
    EXPECT_EQ(source.image->mips[0].data[4], 40u);
}

TEST(CookSourceTest, ParseKtx2)
{
    CookImageSource source;
    ASSERT_TRUE(LoadKtx(MakeKtx2(0), source));
    ASSERT_NE(source.image, nullptr);
    EXPECT_EQ(source.image->format, PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(source.image->width, 2u);
}

TEST(CookSourceTest, RejectSupercompressedKtx2)
{
    CookImageSource source;
    EXPECT_FALSE(LoadKtx(MakeKtx2(1), source));
}

TEST(CookCompressTest, Bc7BlockAlignedSize)
{
    ImageBuildConfig config;
    config.encode = ImageEncode::BC7;
    config.srgb   = true;

    // 8x8 BC7 = (8/4)*(8/4) blocks * 16 bytes = 64.
    EXPECT_EQ(CompressOne(config.ResolveFormat(), config, 8, 8), 64u);
    EXPECT_EQ(config.ResolveFormat(), PixelFormat::BC7_SRGB_BLOCK);
}

TEST(CookCompressTest, AstcBlockAlignedSize)
{
    ImageBuildConfig config;
    config.encode    = ImageEncode::ASTC;
    config.astcBlock = 4;
    config.srgb      = true;

    // 9x9 ASTC 4x4 -> ceil(9/4)^2 = 9 blocks * 16 bytes = 144.
    EXPECT_EQ(CompressOne(config.ResolveFormat(), config, 9, 9), 144u);
    EXPECT_EQ(config.ResolveFormat(), PixelFormat::ASTC_4x4_SRGB_BLOCK);
}

TEST(CookCompressTest, Astc8x8Format)
{
    ImageBuildConfig config;
    config.encode    = ImageEncode::ASTC;
    config.astcBlock = 8;
    config.srgb      = false;

    EXPECT_EQ(config.ResolveFormat(), PixelFormat::ASTC_8x8_UNORM_BLOCK);
    // 8x8 -> 1 block * 16 bytes.
    EXPECT_EQ(CompressOne(config.ResolveFormat(), config, 8, 8), 16u);
}

TEST(CookAssetWriterTest, Write2D)
{
    auto image = MakeRGBA8(2, 2);
    ImageAssetData data;
    WriteImageAsset(*image, ImageAssetType::TEXTURE_2D, data);

    EXPECT_EQ(data.type, ImageAssetType::TEXTURE_2D);
    EXPECT_EQ(data.format, PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(data.mipLevels, 1u);
    EXPECT_EQ(data.arrayLayers, 1u);
    ASSERT_EQ(data.slices.size(), 1u);
    EXPECT_EQ(data.slices[0].size, 16u);
    EXPECT_EQ(data.rawData.size(), 16u);
}

TEST(CookAssetWriterTest, WriteCube)
{
    auto image = MakeRGBA8(2, 2, 6);
    ImageAssetData data;
    WriteImageAsset(*image, ImageAssetType::TEXTURE_CUBE, data);

    EXPECT_EQ(data.type, ImageAssetType::TEXTURE_CUBE);
    EXPECT_EQ(data.arrayLayers, 6u);
    ASSERT_EQ(data.slices.size(), 6u);
    EXPECT_EQ(data.slices[5].layer, 5u);
    EXPECT_EQ(data.rawData.size(), 6u * 16u);
}

TEST(CookAssetWriterTest, WriteCompressed)
{
    ImageBuildConfig config;
    config.encode = ImageEncode::BC7;
    config.srgb   = false;

    auto image      = MakeRGBA8(8, 8);
    auto compressed = CompressedImage::CreateFromImageObject(image, config.ResolveFormat());
    ImageCompressor::Payload payload;
    payload.image      = image;
    payload.compressed = compressed;
    payload.config     = config;
    payload.mip        = 0;
    ImageCompressor(payload).DoWork();

    ImageAssetData data;
    WriteImageAsset(*compressed, ImageAssetType::TEXTURE_2D, data);
    EXPECT_EQ(data.format, PixelFormat::BC7_UNORM_BLOCK);
    ASSERT_EQ(data.slices.size(), 1u);
    EXPECT_EQ(data.slices[0].size, 64u);
}

TEST(CookConfigTest, LoadJsonAndResolve)
{
    const char *json = R"({"defaultBundle":"tex_pc","bundles":{"tex_pc":{"encode":"BC7","srgb":true,"maxSize":2048,"generateMip":true},"tex_mobile":{"encode":"ASTC","block":8,"srgb":true,"maxSize":1024,"generateMip":true}}})";
    std::vector<uint8_t> bytes(json, json + std::strlen(json));

    IMemoryArchive in(bytes.data(), bytes.size());
    JsonInputArchive archive(in);

    ImageBuildPresets presets;
    presets.LoadJson(archive);

    ASSERT_EQ(presets.bundles.size(), 2u);

    std::string key;
    const ImageBuildConfig *cfg = presets.Resolve("tex_mobile", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "tex_mobile");
    EXPECT_EQ(cfg->encode, ImageEncode::ASTC);
    EXPECT_EQ(cfg->astcBlock, 8u);
    EXPECT_EQ(cfg->ResolveFormat(), PixelFormat::ASTC_8x8_SRGB_BLOCK);

    cfg = presets.Resolve("missing", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "tex_pc");

    cfg = presets.Resolve("", key);
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(key, "tex_pc");
}
