#include "ShaderSourcesFixture.h"
#include "ShaderArtifactTest.h"

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/rhi/VertexSemantic.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderUsage.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/pipeline/GlobalVariantLayout.h>
#include <cstring>
#include <map>

using namespace sky;
using namespace sky::aurora;

namespace {
    template <typename T>
    struct ReadyAsset final : Asset<T> {
        ReadyAsset()
        {
            this->SetUuid(Uuid::Create());
            this->SetType(Name(AssetTraits<T>::ASSET_TYPE.data()));
            this->status = AssetBase::Status::LOADED;
        }
        void Group(ProductGroupSnapshotPtr value)
        {
            this->productGroup = std::move(value);
        }
    };
    class SkeletalSceneAssets : public testing::Test {
    protected:
        void SetUp() override
        {
            previous = ShaderCompilerFactory::Get().GetCompiler();
            ShaderCompilerFactory::Get().Register(&compiler);
            files = TestShaderSources();
            ASSERT_NE(files, nullptr);
            auto &data           = mesh->Data();
            data                 = ScenePrimitiveMesh(ScenePrimitive::CUBE);
            const auto positions = data.vertexData;
            data.skinInput       = {123, {"root"}};
            data.slotBounds      = {{true, data.bounds.ToAABB()}};
            data.vertexStride    = 32;
            data.vertexSize      = size_t(data.vertexCount) * data.vertexStride;
            data.vertexData.assign(data.vertexSize, 0);
            data.attributes.push_back({uint8_t(VertexSemantic::JOINTS), 0, 0, uint32_t(Format::U_RGBA8), 12});
            data.attributes.push_back({uint8_t(VertexSemantic::WEIGHTS), 0, 0, uint32_t(Format::F_RGBA32), 16});
            for (uint32_t i = 0; i < data.vertexCount; ++i) {
                auto *vertex = data.vertexData.data() + size_t(i) * data.vertexStride;
                std::memcpy(vertex, positions.data() + size_t(i) * sizeof(Vector3), sizeof(Vector3));
                const float weight = 1.f;
                std::memcpy(vertex + 16, &weight, sizeof(weight));
            }
            skin->Data().binding.input = data.skinInput;
            skin->Data().binding.binds = {{Matrix4::Identity(), "root", 0}};
            rig->Data().boneData       = {{Name("root"), INVALID_BONE_ID}};
            rig->Data().refPos.resize(1);
            assets = {{mesh->GetUuid(), mesh}, {skin->GetUuid(), skin}, {rig->GetUuid(), rig}};
            source = CreateSceneAssetSource(files, [this](const Uuid &id) -> AssetPtr {
                const auto found = assets.find(id);
                return found == assets.end() ? nullptr : found->second;
            });
        }
        void TearDown() override
        {
            source.reset();
            ShaderCompilerFactory::Get().Register(previous);
        }
        SceneSkeletalResult Resolve(const Uuid &material = {})
        {
            return source->ResolveSkeletal(mesh->GetUuid(), material, skin->GetUuid(), rig->GetUuid(), {API::VULKAN, 1});
        }
        IShaderCompiler *previous = nullptr;
        ShaderCompilerSlang compiler;
        std::shared_ptr<ShaderFileSystem>     files;
        std::shared_ptr<ReadyAsset<Mesh>>     mesh = std::make_shared<ReadyAsset<Mesh>>();
        std::shared_ptr<ReadyAsset<Skin>>     skin = std::make_shared<ReadyAsset<Skin>>();
        std::shared_ptr<ReadyAsset<Skeleton>> rig  = std::make_shared<ReadyAsset<Skeleton>>();
        std::map<Uuid, AssetPtr>              assets;
        std::shared_ptr<ISceneResourceSource> source;
    };
} // namespace

TEST_F(SkeletalSceneAssets, ProductionMaterialAndReferenceBindingNeedNoAnimationProvider)
{
    const auto result = Resolve();
    if (const auto *error = std::get_if<RendererError>(&result))
        ADD_FAILURE() << error->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneSkeletalResources>>(result));
    const auto ready = std::get<std::shared_ptr<const SceneSkeletalResources>>(result);
    ASSERT_NE(ready, nullptr);
    EXPECT_EQ(ready->binding->key.mesh, mesh->GetUuid());
    EXPECT_EQ(ready->binding->slotToBone.size(), 1u);
    EXPECT_EQ(ready->binding->slotToBone[0], 0u);
    EXPECT_EQ(ready->referenceModels.size(), 1u);
    ASSERT_EQ(ready->geometry->payloads.size(), 2u);
    const auto &techniques = ready->geometry->payloads[1].material->techniques;
    ASSERT_EQ(techniques.size(), 2u);
    for (const auto &technique : techniques) {
        const auto found = std::ranges::find(technique.program.reflection.resources, std::string("gSkinningPalette"), &ShaderResource::name);
        ASSERT_NE(found, technique.program.reflection.resources.end());
        EXPECT_EQ(found->elementStride, sizeof(Vector4));
        EXPECT_EQ(found->bufferKind, ShaderBufferKind::STRUCTURED);
        EXPECT_EQ(found->access, ShaderResourceAccess::READ_ONLY);
    }
    EXPECT_TRUE(std::holds_alternative<RendererError>(source->ResolveMesh(mesh->GetUuid(), {}, {API::VULKAN, 1})));
    source.reset();
    assets.clear();
    mesh.reset();
    skin.reset();
    rig.reset();
    EXPECT_EQ(ready->binding->inverseBindMatrices.size(), 1u);
}

TEST_F(SkeletalSceneAssets, OddTriangleIndexCountPadsUploadWithoutChangingDrawRanges)
{
    auto &data = mesh->Data();
    data.indexCount = 3;
    data.indexSize = 3 * sizeof(uint16_t);
    data.indexData.resize(data.indexSize);
    data.subMeshes = {{0, 3, 0}};
    const auto result = Resolve();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneSkeletalResources>>(result));
    const auto &payload = std::get<std::shared_ptr<const SceneSkeletalResources>>(result)->geometry->payloads.front();
    ASSERT_TRUE(payload.mesh);
    const auto &lod = payload.mesh->lods.front();
    EXPECT_EQ(payload.bytes.size() % 4, 0u);
    EXPECT_EQ(lod.size, 6u);
    EXPECT_EQ(lod.command.indexCount, 3u);
    EXPECT_EQ(lod.sections.front().indexCount, 3u);
    EXPECT_EQ(payload.bytes.size(), lod.offset + lod.size + 2);
    EXPECT_EQ(std::memcmp(payload.bytes.data() + lod.offset, data.indexData.data(), data.indexSize), 0);
    EXPECT_EQ(payload.bytes.back(), 0);
}

TEST_F(SkeletalSceneAssets, ProductsOnlySelectionRetainsPipelineAndCompleteVertexInputs)
{
    test::ShaderCacheDirectory products("scene-selection");
    ShaderUsageStore::Clear();
    ShaderUsageStore::SetEnabled(true);
    const auto built = Resolve();
    ShaderUsageStore::SetEnabled(false);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneSkeletalResources>>(built));
    const auto usage = ShaderUsageStore::Snapshot();
    ASSERT_FALSE(usage.empty());
    GlobalVariantLayout pipeline;
    std::string error;
    ASSERT_TRUE(pipeline.Load(*files, "aurora/config/pipeline_variants.slang", &error)) << error;
    ShaderResolver cook(nullptr, products.fs.Get(), *files, 0, 0);
    ASSERT_TRUE(BuildUsageCache(cook, usage, &error, &pipeline.schema)) << error;
    const auto manifest = test::TestProductManifest(DefaultShaderEnvironment(ShaderTarget::SPIRV));
    ASSERT_TRUE(WriteFileAtomic(*products.fs, "distribution.json", {manifest.begin(), manifest.end()}));
    ShaderCompilerFactory::Get().Register(nullptr);
    source = CreateSceneAssetSource(std::make_shared<ShaderFileSystem>(), [this](const Uuid &id) -> AssetPtr {
        const auto found = assets.find(id);
        return found == assets.end() ? nullptr : found->second;
    }, products.fs);
    const auto restored = Resolve();
    if (const auto *failure = std::get_if<RendererError>(&restored))
        ADD_FAILURE() << failure->message;
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneSkeletalResources>>(restored));
    const auto &geometry = *std::get<std::shared_ptr<const SceneSkeletalResources>>(restored)->geometry;
    const auto &meshData = *geometry.payloads.front().mesh;
    const auto mask = RenderVertexMask(meshData.streams, meshData.attributes);
    EXPECT_TRUE(mask.Test(VertexSemantic::POSITION));
    EXPECT_TRUE(mask.Test(VertexSemantic::JOINTS));
    for (const auto &technique : geometry.payloads.back().material->techniques) {
        ASSERT_EQ(technique.variants.size(), 4u);
        EXPECT_TRUE(std::ranges::any_of(technique.variants, [](const auto &variant) {
            return std::ranges::any_of(variant.pipeline.entries, [](const auto &value) { return value.key == Name("SHADOWS") && value.value == 1; });
        }));
        for (const auto &variant : technique.variants)
            EXPECT_EQ(variant.vertexMask, mask.mask);
    }
}

TEST_F(SkeletalSceneAssets, RejectsStaticMaterialBeforePublishingSkeletalTuple)
{
    files->AddVirtualFile("custom/static.slang", R"(
        float4 VSMain(float3 position : POSITION) : SV_Position { return float4(position, 1); }
        float4 FSMain() : SV_Target { return 1; }
    )");
    files->AddVirtualFile("custom/static.slang.json", R"({"entries":["VSMain","FSMain"]})");
    auto                  material = std::make_shared<ReadyAsset<Material>>();
    MaterialTechniqueData technique;
    technique.tag               = "opaque";
    technique.program           = {.vertex   = {.relativePath = "custom/static.slang", .entry = "VSMain"},
                                   .fragment = {.relativePath = "custom/static.slang", .entry = "FSMain"}};
    material->Data().techniques = {technique};
    assets[material->GetUuid()] = material;
    const auto result           = Resolve(material->GetUuid());
    ASSERT_TRUE(std::holds_alternative<RendererError>(result));
    EXPECT_NE(std::get<RendererError>(result).message.find("SkinningBatch"), std::string::npos);
}

TEST_F(SkeletalSceneAssets, CommitCrossingNeverPublishesMixedGenerationFromOneOwner)
{
    auto old         = std::make_shared<ProductGroupSnapshot>();
    old->owner       = mesh->GetUuid();
    old->generation  = Uuid::Create();
    old->revision    = 1;
    auto next        = std::make_shared<ProductGroupSnapshot>(*old);
    next->generation = Uuid::Create();
    next->revision   = 2;
    mesh->Group(old);
    skin->Group(next);
    rig->Group(next);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(Resolve()));
    mesh->Group(next);
    const auto result = Resolve();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const SceneSkeletalResources>>(result));
    const auto ready = std::get<std::shared_ptr<const SceneSkeletalResources>>(result);
    ASSERT_NE(ready, nullptr);
    EXPECT_EQ(ready->binding->key.meshGeneration, next->generation);
    EXPECT_EQ(ready->binding->key.skinGeneration, next->generation);
    EXPECT_EQ(ready->binding->key.skeletonGeneration, next->generation);
}
