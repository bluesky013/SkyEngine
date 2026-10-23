#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/shader/IShaderCompiler.h>
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

TEST(SceneImageConversion, MaterialColorAndLinearDataKeepDifferentNativeFormats)
{
    Asset<Texture> asset;
    asset.SetUuid(Uuid::CreateWithSeed(714));
    auto &data  = asset.Data();
    data.format = PixelFormat::RGBA8_SRGB;
    data.width = data.height = 1;
    data.dataSize            = 4;
    data.slices              = {{0, 4, 0}};
    data.rawData             = {128, 64, 32, 255};
    const auto color         = ConvertSceneImage(asset, {API::METAL, 9}, SceneImageUsage::MATERIAL);
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(color));
    const auto &payload = std::get<RenderResourcePayload>(color);
    ASSERT_TRUE(payload.metadata.image);
    EXPECT_EQ(payload.metadata.image->format, RenderImageFormat::RGBA8_SRGB);
    EXPECT_EQ(payload.bytes, data.rawData);
    EXPECT_EQ(payload.resource.usageVariant, 3u);
    EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneImage(asset, {API::METAL, 9})));

    data.format       = PixelFormat::RGBA8_UNORM;
    const auto linear = ConvertSceneImage(asset, {API::METAL, 9}, SceneImageUsage::MATERIAL);
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(linear));
    EXPECT_EQ(std::get<RenderResourcePayload>(linear).metadata.image->format, RenderImageFormat::RGBA8_UNORM);
    data.rawData[0] = 255;
    EXPECT_EQ(payload.bytes.front(), 128u);
}

namespace {
    class InactiveMaterialCompiler final : public IShaderCompiler {
    public:
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return {"inactive-material-fixture", "1", 1, ShaderReflection::kProtocolVersion, 0};
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            out                       = {};
            out.identity              = {.sourcePath  = desc.sourcePath,
                                         .sourceEntry = desc.entry,
                                         .nativeEntry = desc.entry,
                                         .stage       = desc.stage,
                                         .sourceHash  = desc.sourceHash,
                                         .variantHash = desc.variantHash,
                                         .layoutFp    = desc.layoutFp,
                                         .schemaFp    = desc.schemaFp,
                                         .environment = desc.environment,
                                         .producer    = GetProducerIdentity()};
            auto &reflection          = out.reflection;
            reflection.kind           = ShaderReflectionKind::STAGE;
            reflection.complete       = true;
            reflection.stages         = ShaderStageFlags(desc.stage);
            reflection.bindingModel   = ShaderBindingModel::VULKAN_DESCRIPTORS;
            reflection.declarations   = {{.name             = "inactiveValues",
                                          .type             = ShaderResourceType::UNIFORM_BUFFER,
                                          .set              = 2,
                                          .binding          = 1,
                                          .stages           = ShaderStageFlags(desc.stage),
                                          .declarationOwner = "fixture/inactive"},
                                         {.name             = "inactiveImage",
                                          .type             = ShaderResourceType::SAMPLED_IMAGE,
                                          .set              = 2,
                                          .binding          = 2,
                                          .texture          = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4},
                                          .stages           = ShaderStageFlags(desc.stage),
                                          .declarationOwner = "fixture/inactive"},
                                         {.name             = "inactiveSampler",
                                          .type             = ShaderResourceType::SAMPLER,
                                          .set              = 2,
                                          .binding          = 3,
                                          .sampler          = ShaderSamplerType::REGULAR,
                                          .stages           = ShaderStageFlags(desc.stage),
                                          .declarationOwner = "fixture/inactive"}};
            reflection.declaredBlocks = {
                {"inactiveValues",
                 "Values",
                 2,
                 1,
                 16,
                 {{.name = "tint", .size = 16, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::VECTOR, .cols = 4, .alignment = 16}},
                 16}};
            out.data = {3, 2, 35, 7, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
            return SealShaderArtifact(out, out.errorInfo);
        }
    };
    class LoadedMaterialAsset : public Asset<Material> {
    public:
        LoadedMaterialAsset()
        {
            SetUuid(Uuid::Create());
            SetType(Name(AssetTraits<Material>::ASSET_TYPE.data()));
            status.store(Status::LOADED);
        }
    };
} // namespace

TEST(SceneMaterialSource, InactiveAliasesValidateDeclaredTypesAndSkipImageLoads)
{
    InactiveMaterialCompiler compiler;
    struct Registration {
        IShaderCompiler *previous;
        ~Registration()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } registration{ShaderCompilerFactory::Get().GetCompiler()};
    ShaderCompilerFactory::Get().Register(&compiler);
    const auto resolve = [](MaterialPropertyData property, const std::string &target) {
        auto                  material = std::make_shared<LoadedMaterialAsset>();
        MaterialTechniqueData technique;
        technique.tag               = "opaque";
        technique.program           = {{"fixture/Inactive.slang", "VSMain"}, {"fixture/Inactive.slang", "FSMain"}};
        technique.aliases           = {{property.name, target}};
        material->Data().techniques = {technique};
        material->Data().properties = {std::move(property)};
        auto files                  = std::make_shared<ShaderFileSystem>();
        files->AddVirtualFile("fixture/Inactive.slang", "fixture");
        files->AddVirtualFile("fixture/Inactive.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
        auto source = CreateSceneAssetSource(files, [material](const Uuid &id) -> AssetPtr {
            EXPECT_EQ(id, material->GetUuid()) << "Inactive textures must not invoke the asset loader";
            return id == material->GetUuid() ? material : nullptr;
        });
        return source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), material->GetUuid(), {API::VULKAN, 9});
    };
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(
        resolve({"value", MaterialPropertyType::FLOAT4, Vector4{1}}, "inactiveValues.tint")));
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(
        resolve({"image", MaterialPropertyType::TEXTURE2D, Uuid::Create()}, "inactiveImage")));
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(
        resolve({"filter", MaterialPropertyType::SAMPLER, Sampler::Descriptor{}}, "inactiveSampler")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"value", MaterialPropertyType::FLOAT4, Vector4{1}}, "inactiveSampler")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"value", MaterialPropertyType::INT, int32_t{1}}, "inactiveValues.tint")));
    EXPECT_TRUE(
        std::holds_alternative<RendererError>(resolve({"filter", MaterialPropertyType::SAMPLER, Sampler::Descriptor{}}, "inactiveValues.tint")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"image", MaterialPropertyType::TEXTURE2D, Uuid::Create()}, "inactiveSampler")));
}
