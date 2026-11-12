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
            EXPECT_EQ(parameters.Resolve(Name("gSceneBatch.world"), shared::SceneBatchBindings::gSceneBatch.set, handle),
                      ShaderParameterLookup::READY);
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

TEST(SceneViewSettingsTest, ScenePolicyRequestsOnlyRegisteredChoices)
{
    ShaderVariantSchema schema;
    schema.sources   = {{Name("pipeline"), 0, 34}};
    schema.totalBits = 34;
    schema.entries   = {
        {Name("SHADOWS"), Name("pipeline"), 0, 1, 0}, {Name("IBL"), Name("pipeline"), 1, 1, 0}, {Name("CUSTOM"), Name("pipeline"), 2, 32, 17}};
    for (auto &entry : schema.entries)
        entry.domain = ShaderVariantDomain::PIPELINE;
    ASSERT_TRUE(schema.Validate());
    const auto requests = ScenePipelineRequests(schema);
    ASSERT_EQ(requests.size(), 4u);
    for (const auto &request : requests) {
        ShaderVariantSelection selection;
        std::string            error;
        ASSERT_TRUE(ResolveShaderVariant(schema, {}, &request, nullptr, selection, error)) << error;
        const auto custom = std::ranges::find(selection.identity, "CUSTOM", &ShaderVariantValue::name);
        ASSERT_NE(custom, selection.identity.end());
        EXPECT_EQ(custom->value, 17u);
    }
    schema.entries.erase(schema.entries.begin(), schema.entries.begin() + 2);
    EXPECT_EQ(ScenePipelineRequests(schema).size(), 1u);
}

TEST(SceneViewSettingsTest, ScenePassAdvertisesStageSupport)
{
    EXPECT_TRUE(CreateSceneRenderFeature(Name("scene"))->SupportsSceneExtensions());
}

namespace {
    class SceneStageContext final : public FrameGraphBuildContext, public FrameFeaturePrepareContext {
    public:
        SceneStageContext()
        {
            frame           = std::make_shared<PreparedFrame>();
            inputs.prepared = frame;
            target.output   = {{0, 1}, 1, {32, 24}, PixelFormat::RGBA8_UNORM};
            target.color    = {1, 0};
        }
        RendererInfo GetRendererInfo() const override
        {
            return {API::VULKAN, 1};
        }
        const FrameBuildInputs &GetInputs() const override
        {
            return inputs;
        }
        std::optional<FrameRenderTarget> GetOutput(ViewportId id) const override
        {
            return id == target.output.viewport ? std::optional{target} : std::nullopt;
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
        std::variant<FrameError, RasterUniformBinding> WriteUniform(uint32_t, uint32_t, std::span<const std::byte>, ShaderResourceType) override
        {
            return Unsupported();
        }
        std::variant<FrameError, std::vector<ResolvedRasterDraw>> ResolveDraws(Name, Rect2D, PreparedDrawList, const ShaderVariant *) override
        {
            return std::vector<ResolvedRasterDraw>{};
        }
        std::variant<FrameError, GraphBufferHandle> CreateBuffer(const Buffer::Descriptor &) override
        {
            return Unsupported();
        }
        std::variant<FrameError, GraphImageHandle> CreateImage(const Image::Descriptor &desc) override
        {
            images.push_back(desc);
            return GraphImageHandle{1, static_cast<uint32_t>(images.size())};
        }
        FrameOperationResult Copy(BufferCopyPass) override
        {
            return Unsupported();
        }
        FrameOperationResult Copy(BufferImageCopyPass) override
        {
            return Unsupported();
        }
        FrameOperationResult Custom(FrameCustomPass) override
        {
            return Unsupported();
        }
        FrameOperationResult Export(GraphBufferExport) override
        {
            return Unsupported();
        }
        FrameOperationResult Export(GraphImageHandle image) override
        {
            if (image != target.color)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            target.exportRequested = true;
            return FrameOperationComplete{};
        }
        FrameOperationResult Raster(FrameRasterPass pass) override
        {
            passes.push_back(std::move(pass));
            return FrameOperationComplete{};
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
            return {FrameErrorCode::UNSUPPORTED, "Unexpected stage test operation"};
        }
        std::shared_ptr<PreparedFrame> frame;
        FrameBuildInputs               inputs;
        FrameRenderTarget              target;
        std::vector<Image::Descriptor> images;
        std::vector<FrameRasterPass>   passes;
    };
    class SceneExtensionCapture final : public ISceneRenderExtension {
    public:
        Name GetName() const override
        {
            return Name("capture");
        }
        std::optional<RendererError> Validate(const FrameExtensionPayload &) const override
        {
            return {};
        }
        FrameOperationResult
        Build(FrameGraphBuildContext &context, const SceneRenderStageContext &stage, const FrameExtensionPayload &payload) override
        {
            ++calls;
            color = stage.color;
            depth = stage.depth;
            bytes = payload.bytes;
            if (fail)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Stage rejected"};
            FrameRasterPass pass;
            pass.color = color;
            pass.load  = clearColor ? LoadOp::CLEAR : LoadOp::LOAD;
            pass.depth = RasterDepthAttachment{depth, LoadOp::LOAD, StoreOp::STORE, 1.f};
            return context.Raster(std::move(pass));
        }
        bool                 fail       = false;
        bool                 clearColor = false;
        uint32_t             calls      = 0;
        GraphImageHandle     color, depth;
        std::vector<uint8_t> bytes;
    };
} // namespace

TEST(SceneViewSettingsTest, StageConsumesOnlyItsOutputPayloadAndSharesOpaqueAttachments)
{
    SceneStageContext context;
    auto              feature   = CreateSceneRenderFeature(Name("scene"), TestShaderSources());
    auto              extension = std::make_shared<SceneExtensionCapture>();
    context.inputs.sceneExtensions.push_back({Name("scene"), extension});
    context.frame->extensions.push_back({Name("capture"), {1}, {{8, 1}}, {}});
    context.frame->extensions.push_back({Name("capture"), {2}, {{0, 1}}, {}});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(feature->Prepare(context)));
    PreparedViewData view;
    view.view.output = {0, 1};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(feature->BuildView(context, view)));
    ASSERT_EQ(context.passes.size(), 2u);
    ASSERT_TRUE(context.passes.front().depth);
    EXPECT_TRUE(context.target.exportRequested);
    EXPECT_EQ(extension->calls, 1u);
    EXPECT_EQ(extension->bytes, std::vector<uint8_t>{2});
    EXPECT_EQ(extension->color, context.passes.front().color);
    EXPECT_EQ(extension->depth, context.passes.front().depth->image);
    EXPECT_EQ(context.passes.back().load, LoadOp::LOAD);
    EXPECT_EQ(context.passes.back().depth->load, LoadOp::LOAD);
    extension->clearColor = true;
    context.passes.clear();
    EXPECT_TRUE(std::holds_alternative<FrameError>(feature->BuildView(context, view)));
    EXPECT_EQ(context.passes.size(), 1u);
    extension->clearColor = false;
    extension->fail       = true;
    context.passes.clear();
    EXPECT_TRUE(std::holds_alternative<FrameError>(feature->BuildView(context, view)));
    EXPECT_EQ(context.passes.size(), 1u);
}
