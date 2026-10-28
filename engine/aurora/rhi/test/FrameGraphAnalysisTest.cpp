#include <aurora/rdg/FrameBindings.h>
#include <gtest/gtest.h>

using namespace sky::aurora;

namespace {
    class AnalysisBuffer final : public Buffer {
    public:
        explicit AnalysisBuffer(BufferUsageFlags usage) : descriptor{64, usage, MemoryType::GPU_ONLY}
        {
        }
        uint64_t GetSize() const override
        {
            return descriptor.size;
        }
        const Descriptor &GetDescriptor() const noexcept override
        {
            return descriptor;
        }

    private:
        const Descriptor descriptor;
    };
    class AnalysisShader final : public Shader {
    public:
        explicit AnalysisShader(ShaderReflection reflection) : reflection(std::move(reflection))
        {
        }
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }

    private:
        const ShaderReflection reflection;
    };
    class AnalysisCustomEncoder final : public FrameCustomPassEncoder {
    public:
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            return FrameOperationComplete{};
        }
    };
    ShaderPtr UniformShader(ShaderStageFlags stages)
    {
        ShaderReflection reflection;
        ShaderResource   resource;
        resource.name             = "Constants";
        resource.set              = 0;
        resource.binding          = 0;
        resource.count            = 1;
        resource.type             = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        resource.access           = ShaderResourceAccess::READ_ONLY;
        resource.stages           = stages;
        resource.declarationOwner = "rhi-test/framegraph-analysis";
        reflection.resources.push_back(resource);
        reflection.declarations.push_back(resource);
        ShaderBlockLayout block;
        block.name       = "Constants";
        block.structName = "ConstantsData";
        block.set        = 0;
        block.binding    = 0;
        block.size       = 16;
        block.alignment  = 16;
        ShaderBlockMember member;
        member.name       = "value";
        member.size       = 16;
        member.alignment  = 16;
        member.scalarType = ShaderScalarType::FLOAT;
        member.kind       = ShaderTypeKind::VECTOR;
        member.cols       = 4;
        block.members.push_back(member);
        reflection.blocks.push_back(block);
        reflection.declaredBlocks.push_back(block);
        if (stages.TestBit(ShaderStageFlagBit::CS))
            reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        return ShaderPtr(new AnalysisShader(std::move(reflection)));
    }
    ShaderPtr RasterStorageShader()
    {
        ShaderReflection reflection;
        ShaderResource   resource;
        resource.name             = "Storage";
        resource.set              = 0;
        resource.binding          = 0;
        resource.count            = 1;
        resource.type             = ShaderResourceType::STORAGE_BUFFER;
        resource.access           = ShaderResourceAccess::READ_ONLY;
        resource.stages           = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS;
        resource.bufferKind       = ShaderBufferKind::STRUCTURED;
        resource.elementStride    = 16;
        resource.declarationOwner = "rhi-test/framegraph-analysis";
        reflection.resources.push_back(resource);
        reflection.declarations.push_back(resource);
        return ShaderPtr(new AnalysisShader(std::move(reflection)));
    }
    FrameGraphImage ColorTarget()
    {
        FrameGraphImage color;
        color.transient         = true;
        color.final             = ImageLayout::GENERAL;
        color.descriptor.format = PixelFormat::RGBA8_UNORM;
        color.descriptor.extent = {8, 8, 1};
        color.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET;
        color.descriptor.memory = MemoryType::GPU_ONLY;
        return color;
    }
} // namespace

TEST(FrameResourceUses, EffectiveRangesStagesAndCustomOrderAreCompileLocal)
{
    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer            = BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::UNIFORM));
    buffer.size              = 64;
    buffer.initializedRanges = {{16, 16}};
    data.buffers.push_back(buffer);
    FrameComputePass pass;
    pass.shader     = UniformShader(ShaderStageFlagBit::CS);
    pass.sideEffect = true;
    ComputeBindingDraft bindings;
    bindings.buffers = {{0, 0, {{1, 0}, 0, 16}, 16, FrameAccessMode::READ}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(pass, std::move(bindings))));
    data.passes.push_back(pass);
    const auto compute = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(compute));
    const auto &uses = std::get<FrameGraphAnalysis>(compute).passResourceUses[0].buffers;
    ASSERT_EQ(uses.size(), 1u);
    EXPECT_EQ(uses[0].range.offset, 16u);
    EXPECT_EQ(uses[0].range.size, 16u);
    EXPECT_EQ(uses[0].kind, FrameBufferUseKind::UNIFORM);
    EXPECT_EQ(uses[0].access, FrameAccessMode::READ);
    EXPECT_EQ(uses[0].stages, ShaderStageFlags(ShaderStageFlagBit::CS));
    EXPECT_EQ(ResolveComputeBindings(pass).buffers[0].range.offset, 0u);

    data.buffers[0].initializedRanges.clear();
    data.buffers[0].initialized = true;
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::UNIFORM)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    FrameCustomPass custom;
    custom.encoder      = std::make_shared<AnalysisCustomEncoder>();
    custom.sideEffect   = true;
    custom.buffers      = {{0, 9, {{1, 1}, 8, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::UNIFORM},
                           {0, 1, {{1, 0}, 32, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::UNIFORM}};
    data.passes[0]      = custom;
    const auto analyzed = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    const auto &ordered = std::get<FrameGraphAnalysis>(analyzed).passResourceUses[0].buffers;
    ASSERT_EQ(ordered.size(), 2u);
    EXPECT_EQ(ordered[0].range.buffer.index, 1u);
    EXPECT_EQ(ordered[0].range.offset, 8u);
    EXPECT_EQ(ordered[1].range.buffer.index, 0u);
    EXPECT_EQ(ordered[1].range.offset, 32u);
    EXPECT_FALSE(ordered[0].stages);
}

TEST(FrameResourceUses, DeadRasterReflectionIsValidatedBeforeCulling)
{
    for (const bool compute : {false, true}) {
        SCOPED_TRACE(compute);
        FrameGraphData data;
        data.images.push_back(ColorTarget());
        if (compute) {
            FrameComputePass pass;
            pass.shader = UniformShader(ShaderStageFlagBit::CS);
            data.passes.push_back(pass);
        } else {
            FrameRasterPass pass;
            pass.color = {1, 0};
            RasterDraw draw;
            draw.shader  = UniformShader(ShaderStageFlagBit::VS | ShaderStageFlagBit::FS);
            draw.command = {3, 1, 0, 0};
            pass.draws.push_back(draw);
            data.passes.push_back(pass);
        }
        const auto missing = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
        ASSERT_TRUE(std::holds_alternative<FrameError>(missing));
        EXPECT_NE(std::get<FrameError>(missing).message.find("reflection"), std::string::npos);
        ShaderReflection emptyReflection;
        if (compute)
            emptyReflection.threadGroupSize[0] = emptyReflection.threadGroupSize[1] = emptyReflection.threadGroupSize[2] = 1;
        const ShaderPtr empty(new AnalysisShader(std::move(emptyReflection)));
        if (compute)
            std::get<FrameComputePass>(data.passes[0]).shader = empty;
        else
            std::get<FrameRasterPass>(data.passes[0]).draws[0].shader = empty;
        const auto dead = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
        ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(dead));
        const auto &analysis = std::get<FrameGraphAnalysis>(dead);
        EXPECT_TRUE(analysis.livePasses.empty());
        ASSERT_EQ(analysis.passResourceUses.size(), 1u);
        EXPECT_TRUE(analysis.passResourceUses[0].buffers.empty());
        EXPECT_TRUE(analysis.passResourceUses[0].images.empty());
    }
}

TEST(FrameResourceUses, RasterStorageAndVertexReadsKeepTheirProducer)
{
    FrameGraphData   data;
    FrameGraphBuffer storage;
    storage.size = 64;
    storage.transient =
        Buffer::Descriptor{64, BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::STORAGE, MemoryType::GPU_ONLY};
    data.buffers.push_back(storage);
    data.buffers.push_back(
        {BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::TRANSFER_SRC)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    data.passes.push_back(BufferCopyPass{{1, 1}, {1, 0}, 0, 0, 64});
    data.images.push_back(ColorTarget());
    RasterDraw draw;
    draw.shader  = RasterStorageShader();
    draw.command = {3, 1, 0, 0};
    draw.vertices.push_back({{{1, 0}, 0, 24}, 8});
    RasterBindingDraft bindings;
    bindings.storage = {{0, 0, {{1, 0}, 16, 16}, 0}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
    FrameRasterPass raster;
    raster.color = {1, 0};
    raster.draws.push_back(draw);
    data.passes.push_back(raster);
    data.imageExports.push_back({1, 0});
    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(result));
    const auto &analysis = std::get<FrameGraphAnalysis>(result);
    EXPECT_EQ(analysis.livePasses, (std::vector<uint32_t>{0, 1}));
    const auto &uses = analysis.passResourceUses[1].buffers;
    ASSERT_EQ(uses.size(), 2u);
    EXPECT_EQ(uses[0].kind, FrameBufferUseKind::VERTEX);
    EXPECT_EQ(uses[0].range.offset, 0u);
    EXPECT_EQ(uses[0].range.size, 24u);
    EXPECT_EQ(uses[1].kind, FrameBufferUseKind::STORAGE);
    EXPECT_EQ(uses[1].range.offset, 16u);
    EXPECT_EQ(uses[1].range.size, 16u);
    EXPECT_EQ(uses[1].stages, ShaderStageFlags(ShaderStageFlagBit::VS | ShaderStageFlagBit::FS));
    EXPECT_EQ(uses[1].resourceAccess, ShaderResourceAccess::READ_ONLY);
}
