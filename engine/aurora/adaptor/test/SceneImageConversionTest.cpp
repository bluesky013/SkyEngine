#include "SceneAssetFixtures.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

using namespace sky;
using namespace sky::aurora;

TEST(SceneImageConversion, CompleteEnvironmentMipChainKeepsAllCookedBytes)
{
    Asset<Texture> asset;
    asset.SetUuid(Uuid::CreateWithSeed(715));
    auto &data     = asset.Data();
    data.format    = PixelFormat::RGBA8_UNORM;
    data.width     = 256;
    data.height    = 128;
    data.mipLevels = 9;
    for (uint32_t level = 0; level < data.mipLevels; ++level) {
        const auto offset = static_cast<uint32_t>(data.rawData.size());
        const auto size   = std::max(1u, data.width >> level) * std::max(1u, data.height >> level) * 4;
        data.slices.push_back({offset, size, level});
        for (uint32_t i = 0; i < size; ++i)
            data.rawData.push_back(static_cast<uint8_t>((level * 31 + i * 13) % 251 + 1));
    }
    data.dataSize        = data.rawData.size();
    const auto converted = ConvertSceneImage(asset, {API::VULKAN, 9});
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(converted));
    const auto &payload = std::get<RenderResourcePayload>(converted);
    ASSERT_TRUE(payload.metadata.image);
    EXPECT_EQ(payload.metadata.image->mipLevels, 9u);
    EXPECT_EQ(payload.metadata.image->format, RenderImageFormat::RGBA8_UNORM);
    EXPECT_EQ(payload.bytes, data.rawData);
    EXPECT_EQ(payload.bytes.size(), 174764u);
}

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
    auto source = CreateTestSceneAssetSource({}, [pending](const Uuid &) { return pending; });
    EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveImage(Uuid::CreateWithSeed(713), {API::VULKAN, 9})));
    source = CreateTestSceneAssetSource({}, [](const Uuid &) { return AssetPtr{}; });
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

TEST(SceneMaterialSource, IndependentMaterialOwnsTextureVersionsWithoutLoadingAnyMesh)
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
    auto               source = CreateSceneAssetSource(files, [material, texture](const Uuid &id) -> AssetPtr {
        if (id == material->GetUuid())
            return material;
        if (id == texture->GetUuid())
            return texture;
        ADD_FAILURE() << "Independent material must not request an unrelated mesh or asset";
        return {};
    });
    VertexSemanticMask mask;
    mask.Set(VertexSemantic::POSITION);
    const auto resolve = [&] { return source->ResolveMaterial(material->GetUuid(), {API::VULKAN, 9}, mask); };
    const auto first   = resolve();
    if (const auto *error = std::get_if<RendererError>(&first))
        ADD_FAILURE() << error->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMaterialResources>>(first));
    const auto original  = std::get<std::shared_ptr<const SceneMaterialResources>>(first);
    const auto findImage = [](const SceneMaterialResources &resources) -> const RenderResourcePayload * {
        const auto found =
            std::ranges::find_if(resources.dependencies, [](const auto &payload) { return payload && payload->metadata.image.has_value(); });
        return found == resources.dependencies.end() ? nullptr : found->get();
    };
    const auto *oldImage = findImage(*original);
    ASSERT_TRUE(oldImage);
    const auto oldKey          = oldImage->resource;
    texture->Data().rawData[0] = 255;
    ASSERT_TRUE(source->Invalidate(texture->GetUuid()));
    const auto second = resolve();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMaterialResources>>(second));
    const auto  revised  = std::get<std::shared_ptr<const SceneMaterialResources>>(second);
    const auto *newImage = findImage(*revised);
    ASSERT_TRUE(newImage);
    EXPECT_EQ(newImage->resource.product, oldKey.product);
    EXPECT_GT(newImage->resource.revision, oldKey.revision);
    EXPECT_EQ(newImage->bytes.front(), 255u);
    EXPECT_EQ(oldImage->bytes.front(), 128u);
    EXPECT_EQ(oldImage->resource, oldKey);
    bool linked = false;
    ASSERT_TRUE(revised->material.material);
    for (const auto &entry : revised->material.material->techniques)
        for (const auto &image : entry.parameters.images) {
            EXPECT_EQ(image.resource, newImage->resource);
            linked = true;
        }
    EXPECT_TRUE(linked);
}

namespace {
    using namespace std::chrono_literals;

    class GatedMaterialCompiler final : public IShaderCompiler {
    public:
        explicit GatedMaterialCompiler(std::shared_ptr<IShaderCompiler> implementation = std::make_shared<InactiveMaterialCompiler>())
            : compiler(std::move(implementation))
        {
        }
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return compiler->GetProducerIdentity();
        }
        ShaderProducerIdentity GetProducerIdentity(ShaderTarget target) const override
        {
            return compiler->GetProducerIdentity(target);
        }
        bool InspectVariants(const ShaderCompileDesc &desc, std::vector<ShaderVariantBinding> &bindings, std::string &error) override
        {
            if (std::this_thread::get_id() == host) {
                error = "Material shader inspection ran on the host thread";
                return false;
            }
            return compiler->InspectVariants(desc, bindings, error);
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            std::unique_lock lock(mutex);
            ++calls;
            changed.notify_all();
            if (std::this_thread::get_id() == host) {
                out.errorInfo = "Material compilation ran on the host thread";
                return false;
            }
            if (!changed.wait_for(lock, 5s, [&] { return released; }) || fail) {
                out.errorInfo = "Gated shader compilation failed";
                return false;
            }
            lock.unlock();
            return compiler->Compile(desc, out);
        }
        bool WaitForEntry()
        {
            std::unique_lock lock(mutex);
            return changed.wait_for(lock, 5s, [&] { return calls != 0; });
        }
        void Release(bool failCompile = false)
        {
            std::lock_guard lock(mutex);
            fail     = failCompile;
            released = true;
            changed.notify_all();
        }

        std::atomic_uint32_t calls{0};

    private:
        const std::thread::id            host = std::this_thread::get_id();
        std::shared_ptr<IShaderCompiler> compiler;
        std::mutex                       mutex;
        std::condition_variable          changed;
        bool                             released = false, fail = false;
    };

    class AsyncSceneMaterialSource : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(ShaderCompilerFactory::Get().RegisterOwned(compiler));
            files = std::make_shared<ShaderFileSystem>();
            files->AddVirtualFile("aurora/config/pipeline_variants.slang", "// @variant\n// @reserved 16\n// @source pipeline\n// @end\n");
            files->AddVirtualFile("fixture/Scene.slang", "fixture");
            files->AddVirtualFile("fixture/Scene.slang.json", R"({"entries":["VSMain","FSMain"],"variants":[]})");
            MaterialTechniqueData technique;
            technique.tag     = "opaque";
            technique.program = {{"fixture/Scene.slang", "VSMain"}, {"fixture/Scene.slang", "FSMain"}};
            for (const auto &material : materials)
                material->Data().techniques = {technique};
            source = CreateTestSceneAssetSource(
                files,
                [this](const Uuid &id) -> AssetPtr {
                    for (const auto &material : materials)
                        if (material->GetUuid() == id)
                            return material;
                    return {};
                },
                {}, {}, SceneMaterialCompilation::Asynchronous);
        }
        void TearDown() override
        {
            compiler->Release();
            source.reset();
            ShaderCompilerFactory::Get().Unregister(compiler.get());
        }
        SceneResourceResult Resolve(size_t material)
        {
            return source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::CUBE), materials[material]->GetUuid(), {API::VULKAN, 9});
        }
        SceneResourceResult Await(size_t material)
        {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            auto       result   = Resolve(material);
            while (std::holds_alternative<std::monostate>(result) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
                result = Resolve(material);
            }
            return result;
        }

        std::array<std::shared_ptr<LoadedMaterialAsset>, 2> materials{std::make_shared<LoadedMaterialAsset>(),
                                                                      std::make_shared<LoadedMaterialAsset>()};
        std::shared_ptr<GatedMaterialCompiler>              compiler = std::make_shared<GatedMaterialCompiler>();
        std::shared_ptr<ShaderFileSystem>                   files;
        std::shared_ptr<ISceneResourceSource>               source;
    };

    class AsyncVirtualSceneMaterialSource : public AsyncSceneMaterialSource {
    protected:
        void SetUp() override
        {
            compiler = std::make_shared<GatedMaterialCompiler>(std::make_shared<ShaderCompilerSlang>());
            AsyncSceneMaterialSource::SetUp();
            ASSERT_TRUE(files->AddVirtualFile("fixture/Scene.slang", R"(
#define SURFACE_HEADER "fixture/Surface.slangh"
#include SURFACE_HEADER
struct VertexOutput { float4 position : SV_Position; };
[shader("vertex")]
VertexOutput VSMain(float3 position : POSITION)
{
    VertexOutput result;
    result.position = float4(position, 1);
    return result;
}
[shader("fragment")]
float4 FSMain(VertexOutput input) : SV_Target { return SurfaceColor(); }
)"));
            ASSERT_TRUE(files->AddVirtualFile("fixture/Surface.slangh", "float4 SurfaceColor() { return float4(0.25, 0.5, 0.75, 1); }"));
        }
    };
} // namespace

TEST_F(AsyncVirtualSceneMaterialSource, MacroIncludeUsesOwnedSourcesAfterAuthoringChanges)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(Resolve(0)));
    ASSERT_TRUE(files->AddVirtualFile("fixture/Surface.slangh", "#error authored header changed after scheduling"));
    compiler->Release();
    const auto result = Await(0);
    if (const auto *error = std::get_if<RendererError>(&result))
        ADD_FAILURE() << error->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(result));
    EXPECT_EQ(compiler->calls.load(), 2u);
}

TEST_F(AsyncSceneMaterialSource, PendingRequestsShareProgramsAcrossMaterials)
{
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve(0)));
    ASSERT_TRUE(compiler->WaitForEntry());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve(0)));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve(1)));
    compiler->Release();
    const auto first = Await(0), second = Await(1);
    if (const auto *failure = std::get_if<RendererError>(&first))
        ADD_FAILURE() << failure->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(first));
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(second));
    EXPECT_EQ(compiler->calls.load(), 2u); // One VS and one FS for the shared program.
}

TEST_F(AsyncSceneMaterialSource, FailedCompilationIsStableAcrossFramePolls)
{
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve(0)));
    ASSERT_TRUE(compiler->WaitForEntry());
    compiler->Release(true);
    const auto  result  = Await(0);
    const auto *failure = std::get_if<RendererError>(&result);
    ASSERT_NE(failure, nullptr);
    EXPECT_NE(failure->message.find("Gated shader compilation failed"), std::string::npos);
    const auto again = Resolve(0);
    ASSERT_TRUE(std::holds_alternative<RendererError>(again));
    EXPECT_EQ(std::get<RendererError>(again).message, failure->message);
    EXPECT_EQ(compiler->calls.load(), 1u);
}

TEST_F(AsyncSceneMaterialSource, SourceDestructionDrainsActiveCompilation)
{
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve(0)));
    ASSERT_TRUE(compiler->WaitForEntry());
    std::promise<void> closing;
    auto               close = std::async(std::launch::async, [owned = std::move(source), &closing]() mutable {
        closing.set_value();
        owned.reset();
    });
    closing.get_future().wait();
    EXPECT_EQ(close.wait_for(25ms), std::future_status::timeout);
    compiler->Release();
    EXPECT_EQ(close.wait_for(5s), std::future_status::ready);
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
        auto source = CreateTestSceneAssetSource(files, [material](const Uuid &id) -> AssetPtr {
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

TEST(SceneImageConversion, RefreshRetiresWatchesInsteadOfExhaustingLifetimeBudget)
{
    size_t loads  = 0;
    auto   source = CreateTestSceneAssetSource({}, [&](const Uuid &id) -> AssetPtr {
        ++loads;
        auto image = std::make_shared<Asset<Texture>>();
        image->SetUuid(id);
        image->SetType(Name(AssetTraits<Texture>::ASSET_TYPE.data()));
        return image;
    });
    for (uint32_t i = 0; i < 1030; ++i) {
        const auto id = Uuid::CreateWithSeed(50000 + i);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveImage(id, {API::VULKAN, 9})));
        ASSERT_TRUE(source->Invalidate(id));
    }
    EXPECT_EQ(loads, 1030u);
}

TEST(SceneImageConversion, FullAssetBudgetKeepsExistingRequestsPollableAndReportsCapacity)
{
    size_t loads  = 0;
    auto   source = CreateTestSceneAssetSource({}, [&](const Uuid &id) -> AssetPtr {
        ++loads;
        auto image = std::make_shared<Asset<Texture>>();
        image->SetUuid(id);
        image->SetType(Name(AssetTraits<Texture>::ASSET_TYPE.data()));
        return image;
    });
    for (uint32_t i = 0; i < 1024; ++i)
        ASSERT_TRUE(std::holds_alternative<std::monostate>(source->ResolveImage(Uuid::CreateWithSeed(60000 + i), {API::VULKAN, 9})));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveImage(Uuid::CreateWithSeed(60000), {API::VULKAN, 9})));
    const auto blocked = source->ResolveImage(Uuid::CreateWithSeed(70000), {API::VULKAN, 9});
    ASSERT_TRUE(std::holds_alternative<RendererError>(blocked));
    EXPECT_EQ(std::get<RendererError>(blocked).code, RendererErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(loads, 1024u);
}

TEST(SceneMaterialSource, MeshWithoutCookedMaterialFailsWithoutAnyImplicitGlobalLookup)
{
    auto mesh = std::make_shared<LoadedSceneTestAsset<Mesh>>();
    mesh->SetUuid(Uuid::Create());
    mesh->Data() = ScenePrimitiveMesh(ScenePrimitive::CUBE);
    mesh->Data().materials.clear();
    uint32_t   loads  = 0;
    auto       source = CreateSceneAssetSource({}, [mesh, &loads](const Uuid &id) -> AssetPtr {
        ++loads;
        EXPECT_EQ(id, mesh->GetUuid());
        return id == mesh->GetUuid() ? mesh : AssetPtr{};
    });
    const auto result = source->ResolveMesh(mesh->GetUuid(), {}, {API::VULKAN, 9});
    ASSERT_TRUE(std::holds_alternative<RendererError>(result));
    EXPECT_NE(std::get<RendererError>(result).message.find("recook"), std::string::npos);
    EXPECT_EQ(loads, 1u);
}

TEST(SceneMaterialSource, IndependentMaterialReportsPendingMissingAndMismatchedIdentity)
{
    const Uuid         requested = Uuid::Create();
    VertexSemanticMask mask;
    mask.Set(VertexSemantic::POSITION);
    auto pending = std::make_shared<Asset<Material>>();
    pending->SetUuid(requested);
    pending->SetType(Name(AssetTraits<Material>::ASSET_TYPE.data()));
    auto source = CreateSceneAssetSource({}, [pending](const Uuid &) -> AssetPtr { return pending; });
    EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveMaterial(requested, {API::VULKAN, 9}, mask)));
    source = CreateSceneAssetSource({}, [](const Uuid &) -> AssetPtr { return {}; });
    EXPECT_TRUE(std::holds_alternative<RendererError>(source->ResolveMaterial(requested, {API::VULKAN, 9}, mask)));
    auto wrong        = std::make_shared<LoadedMaterialAsset>();
    source            = CreateSceneAssetSource({}, [wrong](const Uuid &) -> AssetPtr { return wrong; });
    const auto result = source->ResolveMaterial(requested, {API::VULKAN, 9}, mask);
    ASSERT_TRUE(std::holds_alternative<RendererError>(result));
    EXPECT_NE(std::get<RendererError>(result).message.find("identity"), std::string::npos);
}

TEST_F(AsyncSceneMaterialSource, MeshesShareMaterialPayloadAndCacheChargesOnlyNewGeometry)
{
    compiler->Release();
    const auto firstResult = Await(0);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(firstResult));
    const auto first        = std::get<std::shared_ptr<const SceneMeshResources>>(firstResult);
    const auto initialBytes = source->CachedCpuPayloadBytes();
    auto       secondResult = source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::PLANE), materials[0]->GetUuid(), {API::VULKAN, 9});
    const auto deadline     = std::chrono::steady_clock::now() + 5s;
    while (std::holds_alternative<std::monostate>(secondResult) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        secondResult = source->ResolveMesh(ScenePrimitiveId(ScenePrimitive::PLANE), materials[0]->GetUuid(), {API::VULKAN, 9});
    }
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(secondResult));
    const auto second = std::get<std::shared_ptr<const SceneMeshResources>>(secondResult);
    ASSERT_EQ(first->payloads.size(), 2u);
    ASSERT_EQ(second->payloads.size(), 2u);
    EXPECT_EQ(first->payloads.back(), second->payloads.back());
    EXPECT_EQ(source->CachedCpuPayloadBytes(), initialBytes + RenderResourceCpuBytes(*second->payloads.front()));
    const auto beforeVariant = source->CachedCpuPayloadBytes();
    const auto variant       = Await(1);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(variant));
    EXPECT_EQ(first->payloads.front(), std::get<std::shared_ptr<const SceneMeshResources>>(variant)->payloads.front());
    EXPECT_GT(source->CachedCpuPayloadBytes(), beforeVariant);
    ASSERT_TRUE(source->Invalidate(materials[0]->GetUuid()));
    const auto refreshedResult = Await(0);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneMeshResources>>(refreshedResult));
    const auto refreshed = std::get<std::shared_ptr<const SceneMeshResources>>(refreshedResult);
    EXPECT_NE(refreshed->payloads.front(), first->payloads.front());
    EXPECT_NE(refreshed->payloads.back(), first->payloads.back());
    EXPECT_EQ(source->CachedCpuPayloadBytes(), initialBytes);
    EXPECT_FALSE(first->payloads.front()->bytes.empty());
}
