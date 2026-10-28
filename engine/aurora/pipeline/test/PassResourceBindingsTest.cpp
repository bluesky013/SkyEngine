#include "GlobalBlock.h"
#include "SceneBatch.h"
#include "SkinningBatch.h"
#include "TextureToScreenPass.h"
#include <algorithm>
#include <aurora/renderer/IRenderFeature.h>
#include <aurora/renderer/PassResourceBindings.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {
    class ReflectedShader final : public Shader {
    public:
        explicit ReflectedShader(ShaderReflection value) : reflection(std::move(value))
        {
        }
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }
        bool EnableNames()
        {
            reflection.kind         = ShaderReflectionKind::PROGRAM;
            reflection.bindingModel = ShaderBindingModel::VULKAN_DESCRIPTORS;
            reflection.complete     = true;
            reflection.stages       = ShaderStageFlagBit::FS;
            for (auto &resource : reflection.resources) {
                resource.stages           = ShaderStageFlagBit::FS;
                resource.declarationOwner = "test/pass-transaction";
                reflection.nativeBindings.push_back({resource.set, resource.binding, ShaderStageFlagBit::FS, resource.binding,
                                                     ShaderNativeCategory::DESCRIPTOR, resource.set, resource.count});
            }
            if (reflection.declarations.empty())
                reflection.declarations = reflection.resources;
            if (reflection.declaredBlocks.empty())
                reflection.declaredBlocks = reflection.blocks;
            for (auto &resource : reflection.declarations)
                resource.declarationOwner = "test/pass-transaction";
            const ShaderParameterAlias alias{"colorAlias", "gTextureToScreen.Misc"};
            return BuildShaderParameterTable(reflection, 123, std::span{&alias, 1}, parameters, error);
        }
        ShaderReflection reflection;
        std::string      error;
    };
    class ParameterFrameContext final : public FrameGraphBuildContext {
    public:
        const FrameBuildInputs &GetInputs() const override
        {
            return inputs;
        }
        std::optional<FrameRenderTarget> GetOutput(ViewportId) const override
        {
            return {};
        }
        std::optional<GraphImageHandle> ResolveViewOutput(ViewOutputRef) const override
        {
            return {};
        }
        std::optional<GraphImageHandle> ResolveImage(const RenderResourceKey &) const override
        {
            return {};
        }
        std::variant<FrameError, ShaderPtr> CreateShader(const RenderTechniqueData &) override
        {
            return Unsupported();
        }
        std::variant<FrameError, SamplerPtr> CreateSampler(const Sampler::Descriptor &) override
        {
            return Unsupported();
        }
        std::variant<FrameError, RasterBufferRange> Upload(std::span<const std::byte>, BufferUsageFlags) override
        {
            return Unsupported();
        }
        std::variant<FrameError, RasterUniformBinding>
        WriteUniform(uint32_t set, uint32_t binding, std::span<const std::byte> bytes, ShaderResourceType) override
        {
            if (failUpload)
                return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Test upload rejected"};
            uploads.emplace_back(bytes.begin(), bytes.end());
            return RasterUniformBinding{set, binding, {{1, 0}, 0, bytes.size()}, 0};
        }
        std::variant<FrameError, std::vector<ResolvedRasterDraw>> ResolveDraws(Name, Rect2D, PreparedDrawList, const ShaderVariant *) override
        {
            return Unsupported();
        }
        std::variant<FrameError, GraphBufferHandle> CreateBuffer(const Buffer::Descriptor &) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Copy(BufferCopyPass) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Copy(BufferImageCopyPass) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Custom(FrameCustomPass) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Export(GraphBufferExport) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameOperationResult Export(GraphImageHandle) override
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        std::variant<FrameError, GraphImageHandle> CreateImage(const Image::Descriptor &) override
        {
            return Unsupported();
        }
        FrameOperationResult Raster(FrameRasterPass) override
        {
            return Unsupported();
        }
        FrameOperationResult Fullscreen(FrameRasterPass) override
        {
            return Unsupported();
        }
        FrameOperationResult Compute(FrameComputePass) override
        {
            return Unsupported();
        }
        static FrameError Unsupported()
        {
            return {FrameErrorCode::UNSUPPORTED, "Unused test operation"};
        }
        FrameBuildInputs                    inputs;
        bool                                failUpload = false;
        std::vector<std::vector<std::byte>> uploads;
    };
    using Slots = shared::TextureToScreenPassBindings;
    RasterBufferRange UniformRange()
    {
        return {{1, 0}, 0, sizeof(shared::TextureToScreenParams)};
    }
    void BindAll(PassResourceBindings &bindings, ShaderResourceType type = ShaderResourceType::UNIFORM_BUFFER)
    {
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Uniform(Slots::gTextureToScreen, UniformRange(), type)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Image(Slots::gSourceTexture, {1, 1})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Sampler(Slots::gSourceSampler, SamplerPtr(new Sampler))));
    }
    FrameOperationResult Apply(RasterDraw &draw, const PassResourceBindings &bindings)
    {
        const PassResourceBindings *groups[]{&bindings};
        return PassResourceBindings::Apply(draw, groups);
    }
} // namespace

TEST(PassResourceBindings, CompleteBindingsFollowActualShaderOrder)
{
    auto reflection = shared::GetTextureToScreenPassReflection();
    std::reverse(reflection.resources.begin(), reflection.resources.end());
    RasterDraw draw;
    draw.shader              = new ReflectedShader(std::move(reflection));
    draw.command.vertexCount = 7;
    PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
    BindAll(bindings);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(Apply(draw, bindings)));
    ASSERT_EQ(ResolveRasterBindings(draw).uniforms.size(), 1u);
    ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
    ASSERT_EQ(ResolveRasterBindings(draw).samplers.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).images.front().image, (GraphImageHandle{1, 1}));
    EXPECT_EQ(draw.command.vertexCount, 7u);
}

TEST(PassResourceBindings, GlobalPassAndBatchComposeWithoutAViewTier)
{
    ShaderReflection reflection;
    for (const auto *schema :
         {&shared::GetGlobalBlockReflection(), &shared::GetTextureToScreenPassReflection(), &shared::GetSceneBatchReflection()}) {
        reflection.resources.insert(reflection.resources.end(), schema->resources.begin(), schema->resources.end());
        reflection.blocks.insert(reflection.blocks.end(), schema->blocks.begin(), schema->blocks.end());
    }
    RasterDraw draw;
    draw.shader = new ReflectedShader(std::move(reflection));
    PassResourceBindings global(shared::GetGlobalBlockReflection()), pass(shared::GetTextureToScreenPassReflection()),
        batch(shared::GetSceneBatchReflection());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        global.Uniform(shared::GlobalBlockBindings::gGlobal, {{1, 2}, 0, sizeof(shared::GlobalParams)})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        batch.Uniform(shared::SceneBatchBindings::gSceneBatch, {{1, 3}, 0, sizeof(shared::SceneBatchParams)})));
    BindAll(pass);
    const PassResourceBindings *groups[]{&global, &pass, &batch};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(draw, groups)));
    ASSERT_EQ(ResolveRasterBindings(draw).uniforms.size(), 3u);
    const auto bound = ResolveRasterBindings(draw);
    for (const uint32_t set : {0u, 1u, shared::SceneBatchBindings::gSceneBatch.set})
        EXPECT_EQ(std::count_if(bound.uniforms.begin(), bound.uniforms.end(), [&](const auto &uniform) { return uniform.set == set; }), 1);
}

TEST(PassResourceBindings, ShaderVariantCanOmitUnusedDeclarations)
{
    auto reflection = shared::GetTextureToScreenPassReflection();
    std::erase_if(reflection.resources, [](const auto &value) { return value.type != ShaderResourceType::UNIFORM_BUFFER; });
    RasterDraw draw;
    draw.shader = new ReflectedShader(std::move(reflection));
    PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Uniform(Slots::gTextureToScreen, UniformRange())));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(Apply(draw, bindings)));
    EXPECT_EQ(ResolveRasterBindings(draw).uniforms.size(), 1u);
    EXPECT_TRUE(ResolveRasterBindings(draw).images.empty());
    EXPECT_TRUE(ResolveRasterBindings(draw).samplers.empty());
}

TEST(PassResourceBindings, MissingBindingsLeaveExistingDrawUnchanged)
{
    for (int missing = 0; missing < 3; ++missing) {
        SCOPED_TRACE(missing);
        RasterDraw draw;
        draw.shader = new ReflectedShader(shared::GetTextureToScreenPassReflection());
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, RasterBindingDraft{.images = {{1, 9, {7, 6}, {}}}})));
        PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
        if (missing != 0)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Uniform(Slots::gTextureToScreen, UniformRange())));
        if (missing != 1)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Image(Slots::gSourceTexture, {1, 1})));
        if (missing != 2)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Sampler(Slots::gSourceSampler, SamplerPtr(new Sampler))));
        EXPECT_TRUE(std::holds_alternative<FrameError>(Apply(draw, bindings)));
        EXPECT_TRUE(ResolveRasterBindings(draw).uniforms.empty());
        ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
        EXPECT_EQ(ResolveRasterBindings(draw).images.front().image, (GraphImageHandle{7, 6}));
        EXPECT_EQ(ResolveRasterBindings(draw).images.front().binding, 9u);
    }
}

TEST(PassResourceBindings, RejectsWrongSlotsDuplicatesAndInvalidRanges)
{
    PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Image({1, 0}, {1, 1})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Image(Slots::gSourceTexture, {})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Sampler(Slots::gSourceSampler, {})));
    auto range = UniformRange();
    --range.size;
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Uniform(Slots::gTextureToScreen, range)));
    range        = UniformRange();
    range.offset = UINT64_MAX;
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Uniform(Slots::gTextureToScreen, range)));
    EXPECT_TRUE(
        std::holds_alternative<FrameError>(bindings.Uniform(Slots::gTextureToScreen, UniformRange(), ShaderResourceType::UNIFORM_BUFFER, 16)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Uniform(Slots::gTextureToScreen, UniformRange())));
    RasterDraw draw;
    draw.shader = new ReflectedShader(shared::GetTextureToScreenPassReflection());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, RasterBindingDraft{.images = {{1, 9, {7, 6}, {}}}})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(Apply(draw, bindings)));
    ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).images.front().binding, 9u);
    PassResourceBindings valid(shared::GetTextureToScreenPassReflection());
    BindAll(valid);
    EXPECT_TRUE(std::holds_alternative<FrameError>(valid.Image(Slots::gSourceTexture, {1, 2})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(valid.Uniform(Slots::gTextureToScreen, UniformRange())));
}

TEST(PassResourceBindings, RejectsShaderShapeLayoutAndDynamicPolicyConflicts)
{
    for (int conflict = 0; conflict < 5; ++conflict) {
        SCOPED_TRACE(conflict);
        auto reflection = shared::GetTextureToScreenPassReflection();
        auto uniform    = std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                       [](const auto &r) { return r.type == ShaderResourceType::UNIFORM_BUFFER; });
        auto image      = std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                       [](const auto &r) { return r.type == ShaderResourceType::SAMPLED_IMAGE; });
        ASSERT_NE(uniform, reflection.resources.end());
        ASSERT_NE(image, reflection.resources.end());
        if (conflict == 0)
            image->texture.arrayed = true;
        if (conflict == 1)
            reflection.blocks.front().members.front().offset += 4;
        if (conflict == 2)
            uniform->type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        if (conflict == 3)
            image->binding = 99;
        if (conflict == 4)
            image->count = 2;
        RasterDraw draw;
        draw.shader = new ReflectedShader(std::move(reflection));
        PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
        BindAll(bindings);
        EXPECT_TRUE(std::holds_alternative<FrameError>(Apply(draw, bindings)));
        EXPECT_TRUE(ResolveRasterBindings(draw).uniforms.empty());
    }
}

TEST(PassResourceBindings, DynamicPolicyIsExplicitAndOverlappingGroupsAreRejected)
{
    auto reflection = shared::GetTextureToScreenPassReflection();
    for (auto &resource : reflection.resources)
        if (resource.type == ShaderResourceType::UNIFORM_BUFFER)
            resource.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
    RasterDraw draw;
    draw.shader = new ReflectedShader(std::move(reflection));
    PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
    BindAll(bindings, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(Apply(draw, bindings)));
    const PassResourceBindings *groups[]{&bindings, &bindings};
    EXPECT_TRUE(std::holds_alternative<FrameError>(PassResourceBindings::Apply(draw, groups)));
}

TEST(PassResourceBindings, DescribedButUnsupportedResourcesFailExplicitly)
{
    auto reflection = shared::GetTextureToScreenPassReflection();
    auto image      = std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                   [](const auto &r) { return r.type == ShaderResourceType::SAMPLED_IMAGE; });
    ASSERT_NE(image, reflection.resources.end());
    image->texture.dimension = ShaderTextureDimension::CUBE;
    RasterDraw draw;
    draw.shader = new ReflectedShader(reflection);
    PassResourceBindings bindings(reflection);
    BindAll(bindings);
    const auto result = Apply(draw, bindings);
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::UNSUPPORTED);
}

TEST(PassResourceBindings, NamedGraphAdapterUsesTheSharedTransactionAndUploadsOnce)
{
    CounterPtr<ReflectedShader> shader(new ReflectedShader(shared::GetTextureToScreenPassReflection()));
    ASSERT_TRUE(shader->EnableNames()) << shader->error;
    const auto                 color = MakeShaderData(Vector4{1, 2, 3, 4});
    ShaderParameterTransaction transaction(*shader, 1);
    ASSERT_TRUE(transaction.WriteData(Name("colorAlias"), color));
    ASSERT_TRUE(transaction.WriteResource(Name("gSourceTexture"), ShaderResourceType::SAMPLED_IMAGE, 0));
    ASSERT_TRUE(transaction.WriteResource(Name("gSourceSampler"), ShaderResourceType::SAMPLER, 1));
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(transaction.Finalize(packet)) << transaction.GetError();
    ASSERT_EQ(packet->uniforms.size(), 1u);
    PassResourceBindings values(shared::GetTextureToScreenPassReflection());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Data(Name("colorAlias"), color)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Image(Name("gSourceTexture"), {1, 1})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Sampler(Name("gSourceSampler"), SamplerPtr(new Sampler))));
    RasterDraw draw;
    draw.shader = shader;
    ParameterFrameContext context;
    PassResourceBindings  unusedGlobal(shared::GetGlobalBlockReflection());
    ASSERT_TRUE(
        std::holds_alternative<FrameOperationComplete>(unusedGlobal.UniformData(shared::GlobalBlockBindings::gGlobal, shared::GlobalParams{})));
    const PassResourceBindings *groups[]{&values, &unusedGlobal};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(context, draw, groups)));
    ASSERT_EQ(context.uploads.size(), 1u);
    EXPECT_EQ(context.uploads.front(), packet->uniforms.front().bytes);
    ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).images.front().image, (GraphImageHandle{1, 1}));
    context.failUpload = true;
    EXPECT_TRUE(std::holds_alternative<FrameError>(PassResourceBindings::Apply(context, draw, groups)));
    EXPECT_EQ(context.uploads.size(), 1u);
    ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).images.front().image, (GraphImageHandle{1, 1}));
}

TEST(PassResourceBindings, StorageTransactionsPreserveExactRangesAndLeaveDrawsIntactOnFailure)
{
    auto        reflection = shared::GetTextureToScreenPassReflection();
    const auto &palette    = shared::GetSkinningBatchReflection().resources.front();
    reflection.resources.push_back(palette);
    CounterPtr<ReflectedShader> shader(new ReflectedShader(reflection));
    ASSERT_TRUE(shader->EnableNames()) << shader->error;
    RasterDraw draw;
    draw.shader = shader;
    ParameterFrameContext context;
    PassResourceBindings  material(shared::GetTextureToScreenPassReflection());
    BindAll(material);
    PassResourceBindings    batch(shared::GetSkinningBatchReflection());
    const RasterBufferRange range{{1, 3}, 32, 128};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(batch.Storage(shared::SkinningBatchBindings::gSkinningPalette, range)));
    const PassResourceBindings *groups[]{&material, &batch};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(context, draw, groups)));
    ASSERT_EQ(ResolveRasterBindings(draw).storage.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).storage.front().range.offset, range.offset);
    EXPECT_EQ(ResolveRasterBindings(draw).storage.front().range.size, range.size);
    PassResourceBindings invalid(shared::GetSkinningBatchReflection());
    EXPECT_TRUE(std::holds_alternative<FrameError>(invalid.Storage(shared::SkinningBatchBindings::gSkinningPalette, {{1, 3}, 0, 1})));
    groups[1] = &invalid;
    EXPECT_TRUE(std::holds_alternative<FrameError>(PassResourceBindings::Apply(context, draw, groups)));
    ASSERT_EQ(ResolveRasterBindings(draw).storage.size(), 1u);
    EXPECT_EQ(ResolveRasterBindings(draw).storage.front().range.offset, range.offset);
    EXPECT_EQ(ResolveRasterBindings(draw).storage.front().range.size, range.size);
}

TEST(PassResourceBindings, SharedTransactionRejectsWrongTypesAndOverlappingProvidersBeforeUpload)
{
    CounterPtr<ReflectedShader> shader(new ReflectedShader(shared::GetTextureToScreenPassReflection()));
    ASSERT_TRUE(shader->EnableNames()) << shader->error;
    for (int failure = 0; failure < 3; ++failure) {
        SCOPED_TRACE(failure);
        PassResourceBindings values(shared::GetTextureToScreenPassReflection());
        const auto           color = MakeShaderData(Vector4{1, 2, 3, 4});
        ASSERT_TRUE(
            std::holds_alternative<FrameOperationComplete>(values.Data(Name("colorAlias"), failure == 0 ? MakeShaderData(uint32_t(1)) : color)));
        if (failure == 1)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Data(Name("gTextureToScreen.Misc"), color)));
        if (failure == 2)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.UniformData(Slots::gTextureToScreen, shared::TextureToScreenParams{})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Image(Name("gSourceTexture"), {1, 1})));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(values.Sampler(Name("gSourceSampler"), SamplerPtr(new Sampler))));
        RasterDraw draw;
        draw.shader = shader;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, RasterBindingDraft{.images = {{1, 9, {7, 6}, {}}}})));
        ParameterFrameContext       context;
        const PassResourceBindings *groups[]{&values};
        EXPECT_TRUE(std::holds_alternative<FrameError>(PassResourceBindings::Apply(context, draw, groups)));
        EXPECT_TRUE(context.uploads.empty());
        ASSERT_EQ(ResolveRasterBindings(draw).images.size(), 1u);
        EXPECT_EQ(ResolveRasterBindings(draw).images.front().binding, 9u);
    }
}
