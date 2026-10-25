#include <algorithm>
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
        explicit InactiveMaterialCompiler(bool activeImage = false) : activeImage(activeImage)
        {
        }
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return {"inactive-material-fixture", activeImage ? "active-image" : "1", 1, ShaderReflection::kProtocolVersion, 0};
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
            if (activeImage) {
                reflection.blocks         = {reflection.declaredBlocks[0]};
                reflection.resources      = {reflection.declarations[0], reflection.declarations[1]};
                reflection.nativeBindings = {
                    {.set = 2, .binding = 1, .stage = desc.stage, .index = 1, .category = ShaderNativeCategory::DESCRIPTOR, .space = 2},
                    {.set = 2, .binding = 2, .stage = desc.stage, .index = 2, .category = ShaderNativeCategory::DESCRIPTOR, .space = 2}};
            }
            out.data = {3, 2, 35, 7, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
            return SealShaderArtifact(out, out.errorInfo);
        }

    private:
        bool activeImage = false;
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
    class LoadedTextureAsset : public Asset<Texture> {
    public:
        LoadedTextureAsset()
        {
            SetUuid(Uuid::Create());
            SetType(Name(AssetTraits<Texture>::ASSET_TYPE.data()));
            status.store(Status::LOADED);
            auto &data = Data();
            data.width = data.height = 1;
            data.format              = PixelFormat::RGBA8_SRGB;
            data.dataSize            = 4;
            data.slices              = {{0, 4, 0}};
            data.rawData             = {128, 64, 32, 255};
        }
    };
} // namespace

TEST(SceneMaterialSource, TextureInvalidationPublishesANewExactVersionAndPreservesTheOldSnapshot)
{
    InactiveMaterialCompiler compiler(true);
    struct Registration {
        IShaderCompiler *previous;
        ~Registration()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } registration{ShaderCompilerFactory::Get().GetCompiler()};
    ShaderCompilerFactory::Get().Register(&compiler);
    auto                  material = std::make_shared<LoadedMaterialAsset>();
    auto                  texture  = std::make_shared<LoadedTextureAsset>();
    MaterialTechniqueData technique;
    technique.tag               = "opaque";
    technique.program           = {{"fixture/Reload.slang", "VSMain"}, {"fixture/Reload.slang", "FSMain"}};
    technique.aliases           = {{"color", "inactiveImage"}};
    material->Data().techniques = {technique};
    material->Data().properties = {{"color", MaterialPropertyType::TEXTURE2D, texture->GetUuid()}};
    auto files                  = std::make_shared<ShaderFileSystem>();
    files->AddVirtualFile("fixture/Reload.slang", "fixture");
    files->AddVirtualFile("aurora/config/pipeline_variants.slang", "// @variant\n// @reserved 16\n// @source pipeline\n// @end\n");
    files->AddVirtualFile("fixture/Reload.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
    auto       source  = CreateSceneAssetSource(files, [material, texture](const Uuid &id) -> AssetPtr {
        if (id == material->GetUuid())
            return material;
        if (id == texture->GetUuid())
            return texture;
        return {};
    });
    const auto resolve = [&] { return source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), material->GetUuid(), {API::VULKAN, 9}); };
    const auto first   = resolve();
    if (const auto *error = std::get_if<RendererError>(&first))
        ADD_FAILURE() << error->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(first));
    const auto original  = std::get<std::shared_ptr<const SceneMeshResources>>(first);
    const auto findImage = [](const SceneMeshResources &mesh) -> const RenderResourcePayload * {
        const auto found = std::ranges::find_if(mesh.payloads, [](const auto &payload) { return payload.metadata.image.has_value(); });
        return found == mesh.payloads.end() ? nullptr : &*found;
    };
    const auto *oldImage = findImage(*original);
    ASSERT_TRUE(oldImage);
    const auto oldKey          = oldImage->resource;
    texture->Data().rawData[0] = 255;
    ASSERT_TRUE(source->Invalidate(texture->GetUuid()));
    const auto second = resolve();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(second));
    const auto  revised  = std::get<std::shared_ptr<const SceneMeshResources>>(second);
    const auto *newImage = findImage(*revised);
    ASSERT_TRUE(newImage);
    EXPECT_EQ(newImage->resource.product, oldKey.product);
    EXPECT_GT(newImage->resource.revision, oldKey.revision);
    EXPECT_EQ(newImage->bytes.front(), 255u);
    EXPECT_EQ(oldImage->bytes.front(), 128u);
    EXPECT_EQ(oldImage->resource, oldKey);
    bool linked = false;
    for (const auto &payload : revised->payloads)
        if (payload.material)
            for (const auto &entry : payload.material->techniques)
                for (const auto &image : entry.parameters.images) {
                    EXPECT_EQ(image.resource, newImage->resource);
                    linked = true;
                }
    EXPECT_TRUE(linked);
}

TEST(SceneMaterialSource, InactiveDeclarationsPreserveTypedValues)
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
        files->AddVirtualFile("aurora/config/pipeline_variants.slang", "// @variant\n// @reserved 16\n// @source pipeline\n// @end\n");
        files->AddVirtualFile("fixture/Inactive.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
        auto source = CreateSceneAssetSource(files, [material](const Uuid &id) -> AssetPtr {
            EXPECT_EQ(id, material->GetUuid()) << "Inactive textures must not invoke the asset loader";
            return id == material->GetUuid() ? material : nullptr;
        });
        return source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), material->GetUuid(), {API::VULKAN, 9});
    };
    EXPECT_FALSE(std::holds_alternative<RendererError>(resolve({"value", MaterialPropertyType::FLOAT4, Vector4{1}}, "inactiveValues.tint")));
    EXPECT_FALSE(std::holds_alternative<RendererError>(resolve({"image", MaterialPropertyType::TEXTURE2D, Uuid::Create()}, "inactiveImage")));
    EXPECT_FALSE(std::holds_alternative<RendererError>(resolve({"filter", MaterialPropertyType::SAMPLER, Sampler::Descriptor{}}, "inactiveSampler")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"value", MaterialPropertyType::FLOAT4, Vector4{1}}, "inactiveSampler")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"value", MaterialPropertyType::INT, int32_t{1}}, "inactiveValues.tint")));
    EXPECT_TRUE(
        std::holds_alternative<RendererError>(resolve({"filter", MaterialPropertyType::SAMPLER, Sampler::Descriptor{}}, "inactiveValues.tint")));
    EXPECT_TRUE(std::holds_alternative<RendererError>(resolve({"image", MaterialPropertyType::TEXTURE2D, Uuid::Create()}, "inactiveSampler")));
}

TEST(SceneMaterialSource, DefinitionGeneratesKeysAndMigratesExactAliasesTransactionally)
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
    ShaderFileSystem files;
    files.AddVirtualFile("fixture/Definition.slang", "fixture");
    files.AddVirtualFile("fixture/Definition.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
    MaterialAssetData     source;
    MaterialTechniqueData technique;
    technique.tag     = "opaque";
    technique.program = {{"fixture/Definition.slang", "VSMain"}, {"fixture/Definition.slang", "FSMain"}};
    technique.aliases = {{"color", "inactiveValues.tint"}};
    source.techniques = {technique};
    source.properties = {{"color", MaterialPropertyType::FLOAT4, Vector4{1}}};
    MaterialDefinitionData definition;
    std::string            error;
    ASSERT_TRUE(ResolveMaterialDefinition(source, files, API::VULKAN, nullptr, definition, error)) << error;
    EXPECT_EQ(definition.schema.keys.size(), 3u);
    EXPECT_TRUE(definition.canonical.techniques[0].aliases.empty());
    ASSERT_EQ(definition.canonical.properties.size(), 1u);
    EXPECT_EQ(definition.canonical.properties[0].name, "inactiveValues.tint");
    source.techniques[0].aliases[0].target = "unrelated.tint";
    EXPECT_FALSE(ResolveMaterialDefinition(source, files, API::VULKAN, nullptr, definition, error));
    EXPECT_EQ(definition.canonical.properties[0].name, "inactiveValues.tint");
}
