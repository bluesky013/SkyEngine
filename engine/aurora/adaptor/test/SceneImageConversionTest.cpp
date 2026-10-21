#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <cstring>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

TEST(SceneImageConversion, OwnsReorderedMipBytesAndRejectsInvalidRadianceOrSlices)
{
    Asset<Texture> asset;
    asset.SetUuid(Uuid::CreateWithSeed(712));
    auto &data     = asset.Data();
    data.format    = PixelFormat::RGBA16_SFLOAT;
    data.width     = 2;
    data.mipLevels = 2;
    data.dataSize  = 24;
    data.slices    = {{16, 8, 1}, {0, 16, 0}};
    data.rawData.resize(24);
    for (size_t offset = 0; offset < data.rawData.size(); offset += 2) {
        const uint16_t half = offset < 16 ? 0x4400 : 0x3800;
        std::memcpy(data.rawData.data() + offset, &half, 2);
    }
    const auto result = ConvertSceneImage(asset, {API::VULKAN, 9});
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(result));
    const auto &payload = std::get<RenderResourcePayload>(result);
    EXPECT_EQ(payload.bytes, data.rawData);
    EXPECT_EQ(payload.metadata.image->format, RenderImageFormat::RGBA16_FLOAT);
    const auto original = data;
    for (uint16_t invalid : {uint16_t{0x7c00}, uint16_t{0x7e00}, uint16_t{0xbc00}}) {
        std::memcpy(data.rawData.data(), &invalid, 2);
        EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneImage(asset, {API::VULKAN, 9})));
    }
    EXPECT_EQ(payload.bytes, original.rawData);
    data                  = original;
    data.slices[0].offset = 8;
    EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneImage(asset, {API::VULKAN, 9})));
    data                    = original;
    data.slices[0].mipLevel = 0;
    EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneImage(asset, {API::VULKAN, 9})));
    data      = original;
    data.type = ImageAssetType::TEXTURE_CUBE;
    EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneImage(asset, {API::VULKAN, 9})));
}

TEST(SceneImageConversion, PendingAndMissingAssetsRemainExplicit)
{
    auto pending = std::make_shared<Asset<Texture>>();
    pending->SetType(Name(AssetTraits<Texture>::ASSET_TYPE.data()));
    auto source = CreateSceneAssetSource({}, [pending](const Uuid &) { return pending; });
    EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveImage(Uuid::CreateWithSeed(713), {API::VULKAN, 9})));
    source = CreateSceneAssetSource({}, [](const Uuid &) { return AssetPtr{}; });
    EXPECT_TRUE(std::holds_alternative<RendererError>(source->ResolveImage(Uuid::CreateWithSeed(713), {API::VULKAN, 9})));
}
