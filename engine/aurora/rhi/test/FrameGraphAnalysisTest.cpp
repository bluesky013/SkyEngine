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

TEST(FrameResourceUses, IndirectArgumentsAreReadDependenciesAndRequireInitializedContent)
{
    FrameGraphData data;
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::INDIRECT)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    data.images.push_back(ColorTarget());
    RasterDraw draw;
    draw.shader   = ShaderPtr(new AnalysisShader(ShaderReflection{}));
    draw.indirect = RasterIndirectDraw{{{1, 0}, 0, 64}, 1, 0};
    FrameRasterPass raster;
    raster.color = {1, 0};
    raster.load  = LoadOp::CLEAR;
    raster.draws.push_back(draw);
    data.passes.push_back(raster);
    data.imageExports.push_back({1, 0});
    const auto live = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(live));
    const auto &uses = std::get<FrameGraphAnalysis>(live).passResourceUses[0].buffers;
    ASSERT_EQ(uses.size(), 1u);
    EXPECT_EQ(uses[0].kind, FrameBufferUseKind::INDIRECT);
    EXPECT_EQ(uses[0].access, FrameAccessMode::READ);
    EXPECT_EQ(uses[0].range.size, 64u);

    auto invalid                                                                   = data;
    std::get<FrameRasterPass>(invalid.passes[0]).draws[0].indirect->arguments.size = 12;
    const auto tooSmall                                                            = AnalyzeFrameGraph(FrozenFrameGraph(1, invalid));
    ASSERT_TRUE(std::holds_alternative<FrameError>(tooSmall));

    data.buffers[0].initialized = false;
    const auto undefined        = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(undefined));
    EXPECT_NE(std::get<FrameError>(undefined).message.find("undefined"), std::string::npos);
}

TEST(FrameResourceUses, DispatchSelectsExactlyOneCommandAndReadsIndirectArguments)
{
    FrameGraphData data;
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::INDIRECT)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    ShaderReflection reflection;
    reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
    FrameComputePass pass;
    pass.shader     = ShaderPtr(new AnalysisShader(std::move(reflection)));
    pass.dispatch   = ComputeIndirectDispatch{{{1, 0}, 0, 64}};
    pass.sideEffect = true;
    data.passes.push_back(pass);
    const auto live = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(live));
    const auto &uses = std::get<FrameGraphAnalysis>(live).passResourceUses[0].buffers;
    ASSERT_EQ(uses.size(), 1u);
    EXPECT_EQ(uses[0].kind, FrameBufferUseKind::INDIRECT);

    pass.dispatch     = CmdDispatch{1, 1, 1};
    data.passes[0]    = pass;
    const auto direct = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(direct));
    EXPECT_TRUE(std::get<FrameGraphAnalysis>(direct).passResourceUses[0].buffers.empty());
    pass.dispatch  = CmdDispatch{0, 1, 1};
    data.passes[0] = pass;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(IndirectArgumentLayout, NormalizesPackingAndRejectsInvalidShape)
{
    for (const auto kind : {IndirectCommandKind::DRAW, IndirectCommandKind::DRAW_INDEXED, IndirectCommandKind::DISPATCH}) {
        const auto result = ResolveIndirectArgumentLayout(kind);
        ASSERT_TRUE(std::holds_alternative<IndirectArgumentLayout>(result));
        const auto layout = std::get<IndirectArgumentLayout>(result);
        const auto size   = kind == IndirectCommandKind::DRAW ? 16u : kind == IndirectCommandKind::DRAW_INDEXED ? 20u : 12u;
        EXPECT_EQ(layout.stride, size);
        EXPECT_EQ(layout.size, size);
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(kind, 0)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(kind, 1, size - 4)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(kind, 1, size + 1)));
    }
    const auto padded = ResolveIndirectArgumentLayout(IndirectCommandKind::DRAW_INDEXED, 3, 32);
    ASSERT_TRUE(std::holds_alternative<IndirectArgumentLayout>(padded));
    EXPECT_EQ(std::get<IndirectArgumentLayout>(padded).size, 84u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(IndirectCommandKind::DISPATCH, 2)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(IndirectCommandKind::END)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveIndirectArgumentLayout(static_cast<IndirectCommandKind>(UINT8_MAX))));
    const auto large = ResolveIndirectArgumentLayout(IndirectCommandKind::DRAW, UINT32_MAX, UINT32_MAX - 3);
    ASSERT_TRUE(std::holds_alternative<IndirectArgumentLayout>(large));
    EXPECT_EQ(std::get<IndirectArgumentLayout>(large).size, uint64_t(UINT32_MAX - 1) * (UINT32_MAX - 3) + 16);
}

TEST(FrameResourceUses, RejectsMisalignedIndirectStrideAndInsufficientPaddedRange)
{
    FrameGraphData data;
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::INDIRECT)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    data.images.push_back(ColorTarget());
    RasterDraw draw;
    draw.shader   = ShaderPtr(new AnalysisShader(ShaderReflection{}));
    draw.indirect = RasterIndirectDraw{{{1, 0}, 0, 64}, 2, 17};
    FrameRasterPass raster;
    raster.color = {1, 0};
    raster.load  = LoadOp::CLEAR;
    raster.draws.push_back(draw);
    data.passes.push_back(raster);
    data.imageExports.push_back({1, 0});
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    auto &indirect  = *std::get<FrameRasterPass>(data.passes[0]).draws[0].indirect;
    indirect.stride = 48;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    indirect.arguments.size = 63;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(FrameAnalysisBudget, RawDeclarationsAreChargedAcrossDeadPassesBeforeResolution)
{
    FrameGraphData data;
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::UNIFORM)), 64});
    FrameCustomPass pass;
    pass.encoder = std::make_shared<AnalysisCustomEncoder>();
    const FrameBufferBinding binding{0, 0, {{1, 0}, 16, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::UNIFORM};
    pass.buffers.assign(MAX_FRAME_RESOURCE_DECLARATIONS / 2, binding);
    data.passes         = {pass, pass};
    const auto boundary = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(boundary));
    EXPECT_TRUE(std::get<FrameGraphAnalysis>(boundary).livePasses.empty());

    std::get<FrameCustomPass>(data.passes[1]).buffers.push_back(binding);
    std::get<FrameCustomPass>(data.passes[0]).buffers.front().range.buffer = {99, UINT32_MAX};
    const auto exceeded                                                    = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(exceeded));
    const auto &error = std::get<FrameError>(exceeded);
    EXPECT_EQ(error.code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_NE(error.message.find("declaration budget"), std::string::npos);
    EXPECT_NE(error.message.find("pass=1"), std::string::npos);
}

TEST(FrameAnalysisBudget, MipLayerWorkIsChargedBeforeMergingIncludingDeadPasses)
{
    for (const bool live : {false, true}) {
        SCOPED_TRACE(live);
        FrameGraphData data;
        auto           image         = ColorTarget();
        image.descriptor.mipLevels   = 4;
        image.descriptor.arrayLayers = 16;
        image.descriptor.usage       = ImageUsageFlagBit::STORAGE;
        data.images.push_back(image);
        FrameCustomPass pass;
        pass.encoder    = std::make_shared<AnalysisCustomEncoder>();
        pass.sideEffect = live;
        FrameImageBinding binding;
        binding.image  = {1, 0};
        binding.kind   = FrameImageBindingKind::STORAGE;
        binding.access = FrameAccessMode::WRITE;
        binding.view   = {ImageViewType::VIEW_2D_ARRAY, 0, 4, 0, 16};
        pass.images.assign(MAX_FRAME_ACCESS_WORK / 64, binding);
        data.passes.push_back(pass);
        const auto boundary = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
        ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(boundary));
        const auto &analysis = std::get<FrameGraphAnalysis>(boundary);
        EXPECT_EQ(analysis.livePasses.size(), live ? 1u : 0u);
        ASSERT_EQ(analysis.passResourceUses.size(), 1u);
        EXPECT_EQ(analysis.passResourceUses[0].images.size(), live ? pass.images.size() : 0u);
        if (live) {
            const auto &use = analysis.passResourceUses[0].images.back();
            EXPECT_EQ(use.image, binding.image);
            EXPECT_EQ(use.range.levels, 4u);
            EXPECT_EQ(use.range.layers, 16u);
        }

        std::get<FrameCustomPass>(data.passes[0]).images.push_back(binding);
        const auto exceeded = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
        ASSERT_TRUE(std::holds_alternative<FrameError>(exceeded));
        const auto &error = std::get<FrameError>(exceeded);
        EXPECT_EQ(error.code, FrameErrorCode::OUT_OF_MEMORY);
        EXPECT_NE(error.message.find("access work budget"), std::string::npos);
        EXPECT_NE(error.message.find("pass=0"), std::string::npos);
        EXPECT_NE(error.message.find("image=0"), std::string::npos);
    }
}

TEST(FrameResourceUses, MergedContentRangesPreservePartialProducersAndCustomOrder)
{
    FrameGraphData data;
    data.buffers.push_back(
        {BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::TRANSFER_SRC)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    FrameGraphBuffer destination;
    destination.size      = 64;
    destination.transient = Buffer::Descriptor{64, BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_DST, MemoryType::GPU_ONLY};
    data.buffers.push_back(destination);
    data.passes.push_back(BufferCopyPass{{1, 0}, {1, 1}, 0, 0, 32});
    data.passes.push_back(BufferCopyPass{{1, 0}, {1, 1}, 32, 32, 32});
    FrameCustomPass pass;
    pass.encoder    = std::make_shared<AnalysisCustomEncoder>();
    pass.sideEffect = true;
    for (const uint64_t offset : {32u, 16u, 32u})
        pass.buffers.push_back({0, 0, {{1, 1}, offset, 16}, 0, FrameAccessMode::READ_WRITE, FrameBufferBindingKind::STORAGE});
    data.passes.push_back(pass);
    data.exports.push_back({{1, 1}, 0, 64});
    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(result));
    const auto &analysis = std::get<FrameGraphAnalysis>(result);
    EXPECT_EQ(analysis.livePasses, (std::vector<uint32_t>{0, 1, 2}));
    const auto &uses = analysis.passResourceUses[2].buffers;
    ASSERT_EQ(uses.size(), 3u);
    EXPECT_EQ(uses[0].range.offset, 32u);
    EXPECT_EQ(uses[1].range.offset, 16u);
    EXPECT_EQ(uses[2].range.offset, 32u);

    std::get<BufferCopyPass>(data.passes[0]).size = 16;
    const auto missing                            = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(missing));
    const auto &error = std::get<FrameError>(missing);
    EXPECT_NE(error.message.find("pass=2"), std::string::npos);
    EXPECT_NE(error.message.find("buffer=1"), std::string::npos);
    EXPECT_NE(error.message.find("range=[16,48)"), std::string::npos);
}

TEST(FrameResourceUses, DiscardWritesNeverBecomeDefinedContents)
{
    FrameGraphData data;
    data.images.push_back(ColorTarget());
    FrameRasterPass clear;
    clear.color = {1, 0};
    data.passes.push_back(clear);
    auto discard  = clear;
    discard.load  = LoadOp::LOAD;
    discard.store = StoreOp::DONT_CARE;
    data.passes.push_back(discard);
    data.imageExports.push_back({1, 0});
    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    const auto &error = std::get<FrameError>(result);
    EXPECT_NE(error.message.find("Export reads undefined contents"), std::string::npos);
    EXPECT_NE(error.message.find("image=0"), std::string::npos);
    EXPECT_NE(error.message.find("range=[0,64)"), std::string::npos);
}

TEST(FrameAnalysisDiagnostics, InvalidHandlesReportPassAndRangeWithoutDereferencingThem)
{
    FrameGraphData data;
    data.buffers.push_back(
        {BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::TRANSFER_SRC)), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
    data.buffers.push_back({BufferPtr(new AnalysisBuffer(BufferUsageFlagBit::TRANSFER_DST)), 64});
    data.passes.push_back(BufferCopyPass{{1, 0}, {1, 1}, 0, 0, 16});
    data.passes.push_back(BufferCopyPass{{42, UINT32_MAX}, {1, 1}, 8, 0, 16});
    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    const auto &error = std::get<FrameError>(result);
    EXPECT_EQ(error.code, FrameErrorCode::COMPILE_FAILED);
    for (const auto *part : {"pass=1", "buffer=4294967295", "graph=42", "offset=8", "size=16"})
        EXPECT_NE(error.message.find(part), std::string::npos) << error.message;
}

TEST(FrameAccessBoundaries, LastValuesAreValidButEndAndOutOfRangeAreRejected)
{
    FrameBufferBinding buffer;
    buffer.range  = {{1, 0}, 0, 16};
    buffer.kind   = FrameBufferBindingKind::STORAGE;
    buffer.access = FrameAccessMode::READ_WRITE;
    ASSERT_TRUE(std::holds_alternative<RasterBufferRange>(ResolveFrameBufferRange(buffer, 64)));
    for (const auto kind : {FrameBufferBindingKind::END, static_cast<FrameBufferBindingKind>(UINT8_MAX)}) {
        buffer.kind = kind;
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveFrameBufferRange(buffer, 64)));
    }
    buffer.kind = FrameBufferBindingKind::STORAGE;
    for (const auto access : {FrameAccessMode::END, static_cast<FrameAccessMode>(UINT8_MAX)}) {
        buffer.access = access;
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveFrameBufferRange(buffer, 64)));
    }
    FrameImageBinding image;
    image.kind            = FrameImageBindingKind::STORAGE;
    image.access          = FrameAccessMode::READ_WRITE;
    const auto descriptor = ColorTarget().descriptor;
    ASSERT_TRUE(std::holds_alternative<RasterImageView>(ResolveFrameImageView(image, descriptor, true)));
    for (const auto kind : {FrameImageBindingKind::END, static_cast<FrameImageBindingKind>(UINT8_MAX)}) {
        image.kind = kind;
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveFrameImageView(image, descriptor, true)));
    }
    image.kind = FrameImageBindingKind::STORAGE;
    for (const auto access : {FrameAccessMode::END, static_cast<FrameAccessMode>(UINT8_MAX)}) {
        image.access = access;
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveFrameImageView(image, descriptor, true)));
    }
}

TEST(FrameAnalysisDiagnostics, InvalidImageCopyReportsRequestedMipAndLayer)
{
    FrameGraphData data;
    data.images.push_back(ColorTarget());
    BufferImageCopyPass copy;
    copy.image                     = {1, 0};
    copy.region.subRange.level     = 7;
    copy.region.subRange.baseLayer = 9;
    data.passes.push_back(copy);
    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    const auto &error = std::get<FrameError>(result);
    EXPECT_EQ(error.code, FrameErrorCode::COMPILE_FAILED);
    for (const auto *part : {"pass=0", "image=0", "mip=7", "layer=9"})
        EXPECT_NE(error.message.find(part), std::string::npos) << error.message;
}
