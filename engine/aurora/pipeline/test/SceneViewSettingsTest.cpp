#include "SceneBatch.h"
#include "SelectionMaskPass.h"
#include "SelectionOutlinePass.h"
#include "ShaderSourcesFixture.h"
#include <algorithm>
#include <aurora/pipeline/SceneHighlight.h>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <aurora/renderer/PassResourceBindings.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <string_view>

using namespace sky;
using namespace sky::aurora;

TEST(SceneViewSettingsTest, RejectsMalformedAndNonFiniteDisplayInputs)
{
    const Name name("scene");
    auto       feature = CreateSceneRenderFeature(name);
    auto       input   = SceneViewInput(name, {0, 1}, {1, 0});
    EXPECT_FALSE(feature->Validate(input));
    for (float exposure : {-17.f, 17.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_TRUE(feature->Validate(SceneViewInput(name, {0, 1}, {1, exposure})));
    EXPECT_TRUE(feature->Validate(SceneViewInput(name, {0, 1}, {2, 0})));
    input.bytes.pop_back();
    EXPECT_TRUE(feature->Validate(input));
    input = SceneViewInput(name, {0, 1}, {});
    input.outputs.clear();
    EXPECT_TRUE(feature->Validate(input));
}

TEST(SceneViewSettingsTest, HighlightsRejectInvalidStyleIdentityAndPayload)
{
    const Name                   name("scene");
    const auto                   feature = CreateSceneRenderFeature(name);
    std::vector<RenderEntityRef> entities{{{0, 1}, {0, 1}}, {{0, 1}, {1, 1}}};
    EXPECT_FALSE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    EXPECT_FALSE(feature->Validate(SceneHighlightInput(name, {0, 1}, {})));
    SceneHighlightStyle style;
    for (float width : {0.f, 5.f, std::numeric_limits<float>::quiet_NaN()}) {
        style.width = width;
        EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities, style)));
    }
    style            = {};
    style.occluded.w = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities, style)));
    entities.back() = entities.front();
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    entities.back().entity.generation = 0;
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    auto input = SceneHighlightInput(name, {0, 1}, {});
    input.bytes.pop_back();
    EXPECT_TRUE(feature->Validate(input));
    entities.resize(8193);
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
}

namespace {
    class SceneHighlightUniforms : public testing::TestWithParam<ShaderTarget> {};

    template <class T>
    void CheckSelectionUniform(const ShaderReflection &reflection, ShaderUniformSlot<T> slot, const T &value)
    {
        const auto block =
            std::ranges::find_if(reflection.blocks, [&](const auto &item) { return item.set == slot.set && item.binding == slot.binding; });
        ASSERT_NE(block, reflection.blocks.end());
        const auto       &cpu = T::Schema();
        UniformUploadPlan plan;
        std::string       error;
        ASSERT_TRUE(BuildUniformUploadPlan(cpu, *block, 1, plan, error)) << error;
        std::vector<std::byte> encoded;
        const auto             source = std::as_bytes(std::span{&value, 1});
        ASSERT_TRUE(EncodeShaderUniform(plan, source, encoded, error)) << error;
        ASSERT_EQ(encoded.size(), block->size);
        for (const auto &field : cpu.fields) {
            const auto target = std::ranges::find(block->members, field.name, &ShaderBlockMember::name);
            ASSERT_NE(target, block->members.end());
            const bool matrix = field.kind == ShaderTypeKind::MATRIX;
            for (uint32_t row = 0; row < field.rows; ++row) {
                for (uint32_t column = 0; column < field.cols; ++column) {
                    const auto sourceOffset = field.offset + (matrix ? column * field.matrixStride + row * 4 : column * 4);
                    const auto targetOffset = target->offset + (matrix ? (target->matrixLayout == ShaderMatrixLayout::COLUMN_MAJOR
                                                                              ? column * target->matrixStride + row * 4
                                                                              : row * target->matrixStride + column * 4)
                                                                       : column * 4);
                    ASSERT_LE(sourceOffset + sizeof(float), source.size());
                    ASSERT_LE(targetOffset + sizeof(float), encoded.size());
                    float expected = 0, actual = 0;
                    std::memcpy(&expected, source.data() + sourceOffset, sizeof(float));
                    std::memcpy(&actual, encoded.data() + targetOffset, sizeof(float));
                    EXPECT_FLOAT_EQ(actual, expected) << field.name << '[' << row << ',' << column << ']';
                }
            }
        }
    }
} // namespace

TEST_P(SceneHighlightUniforms, TargetArtifactsEncodeTypedMaskBatchAndOutlineData)
{
    ShaderFileSystem files;
    ASSERT_TRUE(LoadTestShaderSources(files));
    ShaderCompilerSlang compiler;
    Matrix4             projection, world;
    for (uint32_t i = 0; i < 16; ++i) {
        projection.v[i] = float(i + 1);
        world.v[i]      = float(i + 21);
    }
    const shared::SelectionMaskParams    mask{projection};
    const shared::SceneBatchParams       batch{world};
    const shared::SelectionOutlineParams outline{{.1f, .2f, .3f, .4f}, {.5f, .6f, .7f, .8f}, {1280, 720, 3, 0}};
    for (const char *path : {"aurora/pipeline/SelectionMask.slang", "aurora/pipeline/SelectionOutline.slang"}) {
        SCOPED_TRACE(path);
        ShaderReflection program;
        std::string      error;
        for (const bool vertex : {true, false}) {
            ShaderCompileDesc desc;
            ASSERT_TRUE(files.ReadFile(path, desc.source));
            desc.sourcePath = path;
            desc.entry      = vertex ? "VSMain" : "FSMain";
            desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            desc.target     = GetParam();
            desc.fileSystem = &files;
            ShaderCompileResult result;
            ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
            ASSERT_TRUE(MergeShaderReflections(result.reflection, program, error)) << error;
        }
        ShaderParameterTable parameters;
        ASSERT_TRUE(BuildShaderParameterTable(program, 1, {}, parameters, error)) << error;
        ShaderParameterHandle handle;
        if (std::string_view(path).ends_with("SelectionMask.slang")) {
            EXPECT_EQ(parameters.Resolve(Name("gSelectionMask.viewProjection"), 1, handle), ShaderParameterLookup::READY);
            EXPECT_EQ(parameters.Resolve(Name("gSceneBatch.world"), 2, handle), ShaderParameterLookup::READY);
            CheckSelectionUniform(program, shared::SelectionMaskPassBindings::gSelectionMask, mask);
            CheckSelectionUniform(program, shared::SceneBatchBindings::gSceneBatch, batch);
            PassResourceBindings bindings(shared::GetSelectionMaskPassReflection());
            EXPECT_TRUE(
                std::holds_alternative<FrameOperationComplete>(bindings.UniformData(shared::SelectionMaskPassBindings::gSelectionMask, mask)));
        } else {
            EXPECT_EQ(parameters.Resolve(Name("gSelectionOutline.visible"), 1, handle), ShaderParameterLookup::READY);
            EXPECT_EQ(parameters.Resolve(Name("gSelectionOutline.occluded"), 1, handle), ShaderParameterLookup::READY);
            EXPECT_EQ(parameters.Resolve(Name("gSelectionOutline.metrics"), 1, handle), ShaderParameterLookup::READY);
            CheckSelectionUniform(program, shared::SelectionOutlinePassBindings::gSelectionOutline, outline);
            PassResourceBindings bindings(shared::GetSelectionOutlinePassReflection());
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(
                bindings.UniformData(shared::SelectionOutlinePassBindings::gSelectionOutline, outline)));
        }
    }
}

#if defined(_WIN32)
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SceneHighlightUniforms, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL));
#else
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SceneHighlightUniforms, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL));
#endif
