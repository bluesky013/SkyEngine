#include "GlobalBlock.h"
#include "SceneBatch.h"
#include "TextureToScreenPass.h"
#include <algorithm>
#include <aurora/pipeline/PassResourceBindings.h>
#include <gtest/gtest.h>

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
        ShaderReflection reflection;
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
    ASSERT_EQ(draw.uniforms.size(), 1u);
    ASSERT_EQ(draw.images.size(), 1u);
    ASSERT_EQ(draw.samplers.size(), 1u);
    EXPECT_EQ(draw.images.front().image, (GraphImageHandle{1, 1}));
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
    ASSERT_EQ(draw.uniforms.size(), 3u);
    for (uint32_t set = 0; set < 3; ++set)
        EXPECT_EQ(std::count_if(draw.uniforms.begin(), draw.uniforms.end(), [&](const auto &uniform) { return uniform.set == set; }), 1);
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
    EXPECT_EQ(draw.uniforms.size(), 1u);
    EXPECT_TRUE(draw.images.empty());
    EXPECT_TRUE(draw.samplers.empty());
}

TEST(PassResourceBindings, MissingBindingsLeaveExistingDrawUnchanged)
{
    for (int missing = 0; missing < 3; ++missing) {
        SCOPED_TRACE(missing);
        RasterDraw draw;
        draw.shader = new ReflectedShader(shared::GetTextureToScreenPassReflection());
        draw.images.push_back({8, 9, {7, 6}, {}});
        PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
        if (missing != 0)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Uniform(Slots::gTextureToScreen, UniformRange())));
        if (missing != 1)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Image(Slots::gSourceTexture, {1, 1})));
        if (missing != 2)
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(bindings.Sampler(Slots::gSourceSampler, SamplerPtr(new Sampler))));
        EXPECT_TRUE(std::holds_alternative<FrameError>(Apply(draw, bindings)));
        EXPECT_TRUE(draw.uniforms.empty());
        ASSERT_EQ(draw.images.size(), 1u);
        EXPECT_EQ(draw.images.front().image, (GraphImageHandle{7, 6}));
        EXPECT_EQ(draw.images.front().binding, 9u);
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
    BindAll(bindings);
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Image(Slots::gSourceTexture, {1, 2})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(bindings.Uniform(Slots::gTextureToScreen, UniformRange())));
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
        EXPECT_TRUE(draw.uniforms.empty());
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
