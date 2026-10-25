#include "GpuTestSupport.h"
#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrameBindingLookup.h>
#include <common/FrameObjectCache.h>

namespace {
    class ContentBuffer final : public Buffer {
    public:
        uint64_t GetSize() const override
        {
            return 64;
        }
    };
    class AnalysisComputeShader final : public Shader {
    public:
        ShaderReflection        reflection;
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }
    };
    class AnalysisCustomEncoder final : public FrameCustomPassEncoder {
    public:
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            return FrameOperationComplete{};
        }
    };
    ShaderPtr AnalysisShader(ShaderResourceType type, uint32_t binding = 0)
    {
        auto          *shader = new AnalysisComputeShader();
        ShaderResource resource;
        resource.type              = type;
        resource.binding           = binding;
        resource.access            = (type == ShaderResourceType::STORAGE_IMAGE || type == ShaderResourceType::STORAGE_BUFFER)
                                         ? ShaderResourceAccess::READ_WRITE
                                         : ShaderResourceAccess::READ_ONLY;
        if (type == ShaderResourceType::STORAGE_BUFFER) {
            resource.bufferKind    = ShaderBufferKind::STRUCTURED;
            resource.elementStride = 16;
        }
        resource.texture.dimension = ShaderTextureDimension::D2;
        shader->reflection.resources.push_back(resource);
        return ShaderPtr(shader);
    }
    FrameGraphData ComputeBufferData()
    {
        FrameGraphData   data;
        FrameGraphBuffer buffer;
        buffer.buffer = BufferPtr(new ContentBuffer());
        buffer.size   = 64;
        buffer.initializedRanges.push_back({16, 16});
        data.buffers.push_back(std::move(buffer));
        FrameComputePass pass;
        pass.shader = AnalysisShader(ShaderResourceType::UNIFORM_BUFFER_DYNAMIC);
        pass.buffers.push_back({0, 0, {{1, 0}, 0, 16}, 16, FrameAccessMode::READ});
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
        return data;
    }
    class CopyGraph : public testing::TestWithParam<API> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"CopyGraphTest", "SkyEngine", true, GetParam()});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            context.reset(device->CreateFrameContext({2, 1, 61, 2}));
            ASSERT_TRUE(context);
        }
        void TearDown() override
        {
            context.reset();
            Instance::Destroy();
        }
        BufferPtr BufferOf(MemoryType memory, BufferUsageFlags usage, uint64_t size = 64)
        {
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.memory = memory;
            desc.usage  = usage;
            return BufferPtr(device->CreateBuffer(desc));
        }
        GraphBufferHandle Import(FrameGraphBuilder &builder, BufferPtr buffer, CopyBufferState state, bool initialized)
        {
            auto result = builder.Import(std::move(buffer), state, state, initialized);
            if (auto *handle = std::get_if<GraphBufferHandle>(&result))
                return *handle;
            ADD_FAILURE() << "Import failed";
            return {};
        }
        std::shared_ptr<const FrozenFrameGraph> Freeze(FrameGraphBuilder &builder)
        {
            auto result = builder.Freeze();
            if (auto *graph = std::get_if<std::shared_ptr<const FrozenFrameGraph>>(&result))
                return std::move(*graph);
            ADD_FAILURE() << "Freeze failed";
            return {};
        }
        DeviceFrameLease Begin()
        {
            auto result = context->TryBeginSlot();
            if (auto *lease = std::get_if<DeviceFrameLease>(&result))
                return *lease;
            ADD_FAILURE() << "Begin failed";
            return {};
        }
        std::shared_ptr<const FrozenFrameGraph> SimpleGraph(bool exported = true)
        {
            FrameGraphBuilder builder;
            auto source      = Import(builder, BufferOf(MemoryType::CPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC), CopyBufferState::HOST_WRITE, true);
            auto destination = Import(builder, BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_DST), CopyBufferState::COMMON, false);
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, destination, 0, 0, 64})));
            if (exported)
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({destination, 0, 64})));
            return Freeze(builder);
        }
        std::vector<FrameCompletion> Collect(size_t expected)
        {
            std::vector<FrameCompletion> result;
            const auto                   deadline = std::chrono::steady_clock::now() + 5s;
            while (result.size() < expected && std::chrono::steady_clock::now() < deadline) {
                std::array<FrameCompletion, 1> output;
                if (context->PollCompleted(output))
                    result.push_back(std::move(output[0]));
                else
                    std::this_thread::sleep_for(1ms);
            }
            return result;
        }
        Device                             *device = nullptr;
        std::unique_ptr<DeviceFrameContext> context;
    };
    VkResult VKAPI_PTR RejectGraphBegin(VkCommandBuffer, const VkCommandBufferBeginInfo *)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
} // namespace

TEST(FrameComputeAnalysis, DynamicOffsetsDriveBoundsContentsAndProducerLiveness)
{
    auto data = ComputeBufferData();
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.buffers[0].initializedRanges = {{0, 16}};
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.buffers[0].initializedRanges.clear();
    data.buffers[0].initialized                                         = true;
    std::get<FrameComputePass>(data.passes[0]).buffers[0].dynamicOffset = 64;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    auto &binding        = std::get<FrameComputePass>(data.passes[0]).buffers[0];
    binding.range.offset = UINT64_MAX - 7;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));

    data = ComputeBufferData();
    data.buffers[0].initializedRanges.clear();
    FrameGraphBuffer source;
    source.buffer      = BufferPtr(new ContentBuffer());
    source.size        = 64;
    source.initialized = true;
    data.buffers.push_back(std::move(source));
    data.passes.insert(data.passes.begin(), BufferCopyPass{{1, 1}, {1, 0}, 0, 16, 16});
    const auto analyzed = AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{0, 1}));
}

TEST(FrameComputeAnalysis, ShaderBoundariesAndInvalidAccessAreValidated)
{
    for (const auto state : {CopyBufferState::SHADER_READ, CopyBufferState::UNORDERED_ACCESS}) {
        FrameGraphBuilder builder;
        auto              imported = builder.Import(BufferPtr(new ContentBuffer()), state, state, true);
        ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({std::get<GraphBufferHandle>(imported), 0, 16})));
        const auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(*graph)));
    }
    auto  data                = ComputeBufferData();
    auto &compute             = std::get<FrameComputePass>(data.passes[0]);
    compute.buffers[0].access = static_cast<FrameAccessMode>(255);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    FrameCustomPass custom;
    custom.encoder    = std::make_shared<AnalysisCustomEncoder>();
    custom.buffers    = compute.buffers;
    custom.sideEffect = true;
    data.passes[0]    = custom;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(FrameComputeAnalysis, DeclarationErrorsPrecedeCullingButDeadReadsDoNotRequireContents)
{
    auto data = ComputeBufferData();
    data.buffers[0].initializedRanges.clear();
    auto &pass      = std::get<FrameComputePass>(data.passes[0]);
    pass.sideEffect = false;
    const auto dead = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(dead));
    EXPECT_TRUE(std::get<FrameGraphAnalysis>(dead).livePasses.empty());
    EXPECT_TRUE(std::get<FrameGraphAnalysis>(dead).usedBuffers.empty());

    pass.groupsX           = 0;
    const auto invalidPass = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(invalidPass));
    EXPECT_EQ(std::get<FrameError>(invalidPass).code, FrameErrorCode::INVALID_ARGUMENT);
    data.buffers[0].size       = 0;
    const auto invalidResource = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(invalidResource));
    EXPECT_EQ(std::get<FrameError>(invalidResource).code, FrameErrorCode::COMPILE_FAILED);

    data.buffers[0].size = 64;
    pass.groupsX         = 1;
    pass.sideEffect      = true;
    const auto live      = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameError>(live));
    EXPECT_EQ(std::get<FrameError>(live).code, FrameErrorCode::COMPILE_FAILED);
}

TEST(FrameComputeAnalysis, SparseBindingLookupKeepsDeclarationOrderAndDynamicOffsetOrder)
{
    auto            *shader = new AnalysisComputeShader();
    FrameComputePass pass;
    pass.shader = ShaderPtr(shader);
    ShaderResource storage;
    storage.type                 = ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
    storage.set                  = 2;
    storage.binding              = 9;
    ShaderResource uniform       = storage;
    uniform.type                 = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
    uniform.binding              = 1;
    shader->reflection.resources = {storage, uniform};
    pass.buffers = {{.set = 2, .binding = 9, .range = {.buffer = {.graph = 1, .index = 0}, .offset = 0, .size = 16}, .dynamicOffset = 48},
                    {.set = 2, .binding = 1, .range = {.buffer = {.graph = 1, .index = 0}, .offset = 0, .size = 16}, .dynamicOffset = 16}};
    const FrameBindingLookup<ShaderResource> resources(shader->reflection.resources);
    EXPECT_FALSE(resources.HasSet(0));
    EXPECT_FALSE(resources.HasSet(1));
    EXPECT_TRUE(resources.HasSet(2));
    EXPECT_EQ(resources.Find(2, 1), &shader->reflection.resources[1]);
    EXPECT_EQ(resources.Find(2, 8), nullptr);
    EXPECT_EQ(resources.Find(UINT32_MAX, UINT32_MAX), nullptr);
    EXPECT_EQ(ResolveComputeDynamicOffsets(pass, 2), (std::vector<uint32_t>{16, 48}));
    EXPECT_EQ(pass.buffers[0].binding, 9u);
    EXPECT_EQ(pass.buffers[1].binding, 1u);

    const std::vector<ShaderResource>        duplicate{storage, storage};
    const FrameBindingLookup<ShaderResource> duplicates(duplicate);
    EXPECT_EQ(duplicates.Find(2, 9), &duplicate.front());
    shader->reflection.resources = duplicate;
    auto data                    = ComputeBufferData();
    data.passes[0]               = std::move(pass);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data)))));
}

TEST(FrameComputeAnalysis, ViewNormalizationPreservesDescriptorAndCustomPolicies)
{
    Image::Descriptor desc;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {.width = 8, .height = 8, .depth = 1};
    desc.mipLevels   = 3;
    desc.arrayLayers = 2;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::STORAGE;
    FrameImageBinding binding;
    binding.kind       = FrameImageBindingKind::STORAGE;
    binding.view       = {.type = ImageViewType::VIEW_2D_ARRAY, .baseMip = 1, .mipCount = 0, .baseLayer = 1, .layerCount = 1};
    const auto custom  = ResolveFrameImageView(binding, desc);
    const auto sampled = ResolveRasterImageView(binding.view, desc);
    ASSERT_TRUE(std::holds_alternative<RasterImageView>(custom));
    ASSERT_TRUE(std::holds_alternative<RasterImageView>(sampled));
    EXPECT_EQ(std::get<RasterImageView>(custom), std::get<RasterImageView>(sampled));
    EXPECT_EQ(std::get<RasterImageView>(custom).mipCount, 2u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveFrameImageView(binding, desc, true)));
    binding.view.mipCount = 1;
    EXPECT_TRUE(std::holds_alternative<RasterImageView>(ResolveFrameImageView(binding, desc, true)));
    desc.usage = ImageUsageFlagBit::STORAGE;
    EXPECT_TRUE(std::holds_alternative<RasterImageView>(ResolveFrameImageView(binding, desc, true)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveRasterImageView(binding.view, desc)));
}

TEST(FrameComputeAnalysis, StorageImageWritesDefineOnlyTheirSelectedMip)
{
    FrameGraphData  data;
    FrameGraphImage image;
    image.image                = ImagePtr(new Image());
    image.descriptor.extent    = {4, 4, 1};
    image.descriptor.format    = PixelFormat::RGBA8_UNORM;
    image.descriptor.usage     = ImageUsageFlagBit::STORAGE;
    image.descriptor.mipLevels = 2;
    data.images.push_back(std::move(image));
    FrameComputePass pass;
    pass.shader = AnalysisShader(ShaderResourceType::STORAGE_IMAGE);
    pass.images.push_back({0, 0, {1, 0}, {}, FrameAccessMode::WRITE});
    pass.sideEffect = true;
    data.passes.emplace_back(pass);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    pass.images[0].view.mipCount = 1;
    data.passes[0]               = pass;
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.imageExports.push_back({1, 0});
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    pass.images[0].view.baseMip = 1;
    data.passes.emplace_back(pass);
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    pass.images[0].access = FrameAccessMode::READ_WRITE;
    data.passes.emplace_back(pass);
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(FrameComputeAnalysis, CustomStorageReadsRequireContentsAndPreserveTheirProducer)
{
    FrameGraphData  data;
    FrameGraphImage image;
    image.image             = ImagePtr(new Image());
    image.descriptor.extent = {4, 4, 1};
    image.descriptor.format = PixelFormat::RGBA8_UNORM;
    image.descriptor.usage  = ImageUsageFlagBit::STORAGE;
    data.images.push_back(std::move(image));
    FrameCustomPass read;
    read.encoder = std::make_shared<AnalysisCustomEncoder>();
    read.images.push_back({0, 0, {1, 0}, {}, FrameAccessMode::READ, FrameImageBindingKind::STORAGE});
    read.sideEffect = true;
    data.passes.emplace_back(read);
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    FrameComputePass write;
    write.shader = AnalysisShader(ShaderResourceType::STORAGE_IMAGE);
    write.images.push_back({0, 0, {1, 0}, {}, FrameAccessMode::WRITE});
    data.passes.insert(data.passes.begin(), write);
    const auto analyzed = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{0, 1}));
    for (const auto kind : {FrameImageBindingKind::REFLECTED, FrameImageBindingKind::SAMPLED, static_cast<FrameImageBindingKind>(255)}) {
        read.images[0].kind = kind;
        data.passes[1]      = read;
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    }
    read.images[0].kind   = FrameImageBindingKind::SAMPLED;
    read.images[0].access = FrameAccessMode::WRITE;
    data.images[0].descriptor.usage |= ImageUsageFlagBit::SAMPLED;
    data.passes[1] = read;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.passes.pop_back();
    std::get<FrameComputePass>(data.passes[0]).images[0].kind = FrameImageBindingKind::SAMPLED;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(FrameComputeAnalysis, CustomStorageImagesRequireSingleSampleButAllowMultipleMips)
{
    for (bool transient : {false, true}) {
        SCOPED_TRACE(transient);
        for (const auto access : {FrameAccessMode::READ, FrameAccessMode::WRITE, FrameAccessMode::READ_WRITE}) {
            SCOPED_TRACE(static_cast<uint32_t>(access));
            FrameGraphData  data;
            FrameGraphImage image;
            if (!transient)
                image.image = ImagePtr(new Image());
            image.transient            = transient;
            image.final                = ImageLayout::GENERAL;
            image.descriptor.extent    = {4, 4, 1};
            image.descriptor.format    = PixelFormat::RGBA8_UNORM;
            image.descriptor.usage     = ImageUsageFlagBit::STORAGE;
            image.descriptor.memory    = MemoryType::GPU_ONLY;
            image.descriptor.mipLevels = 2;
            data.images.push_back(image);
            FrameCustomPass pass;
            pass.encoder = std::make_shared<AnalysisCustomEncoder>();
            pass.images.push_back({0, 0, {1, 0}, {}, FrameAccessMode::WRITE, FrameImageBindingKind::STORAGE});
            data.passes.emplace_back(pass);
            pass.images[0].access = access;
            pass.sideEffect       = true;
            data.passes.emplace_back(pass);
            ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));

            auto &desc     = data.images[0].descriptor;
            desc.mipLevels = 1;
            for (const auto samples : {SampleCount::X2, SampleCount::X4, SampleCount::X8}) {
                SCOPED_TRACE(static_cast<uint32_t>(samples));
                desc.samples = samples;
                for (bool renderTarget : {false, true}) {
                    SCOPED_TRACE(renderTarget);
                    desc.usage = ImageUsageFlagBit::STORAGE;
                    if (renderTarget)
                        desc.usage |= ImageUsageFlagBit::RENDER_TARGET;
                    const auto result = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
                    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
                    EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::UNSUPPORTED);
                }
            }
        }
    }
}

TEST(FrameComputeAnalysis, ZeroDispatchDimensionsAreInvalid)
{
    for (uint32_t axis = 0; axis < 3; ++axis) {
        auto      data         = ComputeBufferData();
        auto     &pass         = std::get<FrameComputePass>(data.passes[0]);
        uint32_t *dimensions[] = {&pass.groupsX, &pass.groupsY, &pass.groupsZ};
        *dimensions[axis]      = 0;
        const auto result      = AnalyzeFrameGraph(FrozenFrameGraph(1, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::INVALID_ARGUMENT);
    }
}

TEST(FrameComputeAnalysis, CustomBufferKindsAndReadContentsAreIndependent)
{
    auto data = ComputeBufferData();
    data.buffers[0].initializedRanges.clear();
    FrameCustomPass read;
    read.encoder    = std::make_shared<AnalysisCustomEncoder>();
    read.sideEffect = true;
    read.buffers.push_back({0, 0, {{1, 0}, 16, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::STORAGE});
    data.passes[0] = read;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    FrameComputePass write;
    write.shader = AnalysisShader(ShaderResourceType::STORAGE_BUFFER);
    write.buffers.push_back({0, 0, {{1, 0}, 16, 16}, 0, FrameAccessMode::WRITE});
    data.passes.insert(data.passes.begin(), write);
    const auto analyzed = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{0, 1}));
    for (const auto kind : {FrameBufferBindingKind::REFLECTED, static_cast<FrameBufferBindingKind>(255)}) {
        read.buffers[0].kind = kind;
        data.passes[1]       = read;
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    }
    read.buffers[0].kind   = FrameBufferBindingKind::UNIFORM;
    read.buffers[0].access = FrameAccessMode::WRITE;
    data.passes[1]         = read;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    data.passes.pop_back();
    std::get<FrameComputePass>(data.passes[0]).buffers[0].kind = FrameBufferBindingKind::UNIFORM;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));

    data                   = ComputeBufferData();
    data.buffers[0].buffer = {};
    data.buffers[0].initializedRanges.clear();
    data.buffers[0].transient = Buffer::Descriptor{64, BufferUsageFlagBit::UNIFORM, MemoryType::GPU_ONLY};
    data.passes[0]            = write;
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}

TEST(FrameRasterAnalysis, DepthOnlyContentsAndSampledScalarContract)
{
    FrameGraphData  data;
    FrameGraphImage depth;
    depth.transient            = true;
    depth.final                = ImageLayout::GENERAL;
    depth.descriptor.extent    = {8, 8, 1};
    depth.descriptor.mipLevels = 2;
    depth.descriptor.format    = PixelFormat::D32;
    depth.descriptor.usage     = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::SAMPLED;
    data.images.push_back(depth);
    auto color                 = depth;
    color.descriptor.mipLevels = 1;
    color.descriptor.format    = PixelFormat::RGBA8_UNORM;
    color.descriptor.usage     = ImageUsageFlagBit::RENDER_TARGET;
    data.images.push_back(color);
    FrameRasterPass write;
    write.depth = RasterDepthAttachment{{1, 0}};
    data.passes.emplace_back(write);
    FrameRasterPass sample;
    sample.color = {1, 1};
    RasterDraw     draw;
    auto          *shader = new AnalysisComputeShader();
    ShaderResource resource;
    resource.type    = ShaderResourceType::SAMPLED_IMAGE;
    resource.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 1, false, false};
    shader->reflection.resources.push_back(resource);
    draw.shader              = ShaderPtr(shader);
    draw.command.vertexCount = 3;
    draw.images              = {{0, 0, {1, 0}, {ImageViewType::VIEW_2D, 0, 1, 0, 1}}};
    sample.draws.push_back(draw);
    data.passes.emplace_back(sample);
    data.imageExports.push_back({1, 1});
    const auto analyze = [&] { return AnalyzeFrameGraph(FrozenFrameGraph(1, data)); };
    auto       result  = analyze();
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(result));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(result).livePasses, (std::vector<uint32_t>{0, 1}));
    shader->reflection.resources[0].texture.components = 4;
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    shader->reflection.resources[0].texture.components     = 1;
    std::get<FrameRasterPass>(data.passes[0]).depth->store = StoreOp::DONT_CARE;
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    data.passes[0]                                                            = write;
    std::get<FrameRasterPass>(data.passes[1]).draws[0].images[0].view.baseMip = 1;
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    data.passes[1]                                           = sample;
    std::get<FrameRasterPass>(data.passes[0]).colorLocalRead = true;
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    data.passes[0] = FrameRasterPass{};
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    data.passes[0]                                  = write;
    std::get<FrameRasterPass>(data.passes[0]).color = {1, 99};
    EXPECT_TRUE(std::holds_alternative<FrameError>(analyze()));
    depth.descriptor.samples = SampleCount::X4;
    EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveRasterImageView({}, depth.descriptor)));
}

TEST(FrameComputeAnalysis, ConflictingKindsAreRejectedAtNativeStateGranularity)
{
    for (bool custom : {false, true}) {
        SCOPED_TRACE(custom);
        auto data = ComputeBufferData();
        data.buffers[0].initializedRanges.clear();
        data.buffers[0].initialized = true;
        FrameComputePass pass;
        auto            *shader = new AnalysisComputeShader();
        ShaderResource   uniform;
        uniform.type = ShaderResourceType::UNIFORM_BUFFER;
        ShaderResource storage;
        storage.type                 = ShaderResourceType::STORAGE_BUFFER;
        storage.binding              = 1;
        storage.bufferKind           = ShaderBufferKind::STRUCTURED;
        storage.elementStride        = 16;
        shader->reflection.resources = {uniform, storage};
        pass.shader                  = ShaderPtr(shader);
        pass.sideEffect              = true;
        pass.buffers                 = {{0, 0, {{1, 0}, 0, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::UNIFORM},
                                        {0, 1, {{1, 0}, 32, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::STORAGE}};
        auto assign                  = [&] {
            if (custom) {
                FrameCustomPass callback;
                callback.encoder    = std::make_shared<AnalysisCustomEncoder>();
                callback.sideEffect = true;
                callback.buffers    = pass.buffers;
                callback.images     = pass.images;
                data.passes[0]      = callback;
            } else {
                data.passes[0] = pass;
            }
        };
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
        std::reverse(pass.buffers.begin(), pass.buffers.end());
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
        pass.buffers[1].kind                 = FrameBufferBindingKind::STORAGE;
        shader->reflection.resources[0].type         = ShaderResourceType::STORAGE_BUFFER;
        shader->reflection.resources[0].bufferKind    = ShaderBufferKind::STRUCTURED;
        shader->reflection.resources[0].elementStride = 16;
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));

        FrameGraphImage image;
        image.image                = ImagePtr(new Image());
        image.descriptor.extent    = {4, 4, 1};
        image.descriptor.format    = PixelFormat::RGBA8_UNORM;
        image.descriptor.mipLevels = 2;
        image.descriptor.usage     = ImageUsageFlagBit::STORAGE | ImageUsageFlagBit::SAMPLED;
        image.initial = image.final = ImageLayout::GENERAL;
        image.initialized           = true;
        data.images.push_back(image);
        pass.buffers.clear();
        shader->reflection.resources[0].type = ShaderResourceType::SAMPLED_IMAGE;
        shader->reflection.resources[1].type = ShaderResourceType::STORAGE_IMAGE;
        for (auto &resource : shader->reflection.resources)
            resource.texture.dimension = ShaderTextureDimension::D2;
        RasterImageView view;
        view.mipCount = 1;
        pass.images   = {{0, 0, {1, 0}, view, FrameAccessMode::READ, FrameImageBindingKind::SAMPLED},
                         {0, 1, {1, 0}, view, FrameAccessMode::READ, FrameImageBindingKind::STORAGE}};
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
        pass.images[1].view.baseMip = 1;
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
        pass.images[0].kind                  = FrameImageBindingKind::STORAGE;
        shader->reflection.resources[0].type = ShaderResourceType::STORAGE_IMAGE;
        pass.images[1].view.baseMip          = 0;
        assign();
        EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
    }
}

TEST_P(CopyGraph, ResourceFreeCustomExecutesAndRetainsCallbackUntilRetirement)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        bool                 fail  = false;
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            ++calls;
            if (fail)
                return FrameError{FrameErrorCode::ENCODE_FAILED, "Resource-free custom failure"};
            return FrameOperationComplete{};
        }
    };
    for (bool fail : {true, false}) {
        auto first  = std::make_shared<Callback>();
        auto second = std::make_shared<Callback>();
        first->fail = fail;
        FrameGraphData data;
        for (const auto &callback : {first, second}) {
            FrameCustomPass pass;
            pass.encoder    = callback;
            pass.sideEffect = true;
            data.passes.emplace_back(std::move(pass));
        }
        const auto lease    = Begin();
        auto       compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(1, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        EXPECT_EQ(first->calls, 1u);
        EXPECT_EQ(second->calls, fail ? 0u : 1u);
        if (fail) {
            ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
            plan.reset();
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        } else {
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
            auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
            std::weak_ptr<const Callback> weak = first;
            first.reset();
            second.reset();
            plan.reset();
            EXPECT_FALSE(weak.expired());
            EXPECT_EQ(Collect(1).size(), 1u);
            EXPECT_TRUE(weak.expired());
        }
    }
}

TEST_P(CopyGraph, CustomImportedBufferMustSupportItsDeclaredKind)
{
    for (const auto kind : {FrameBufferBindingKind::UNIFORM, FrameBufferBindingKind::STORAGE}) {
        FrameGraphData data;
        data.buffers.push_back(
            {BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC), 64, CopyBufferState::COMMON, CopyBufferState::COMMON, true});
        FrameCustomPass pass;
        pass.encoder    = std::make_shared<AnalysisCustomEncoder>();
        pass.sideEffect = true;
        pass.buffers.push_back({0, 0, {{1, 0}, 0, 16}, 0, FrameAccessMode::READ, kind});
        data.passes.emplace_back(std::move(pass));
        const auto lease    = Begin();
        const auto compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(1, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
        EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_ARGUMENT);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST(FrameBuildLifetime, ArenaRejectsBorrowedResetAndDestroysObjectsInReverseOrder)
{
    struct alignas(64) Object {
        Object(std::vector<int> &order, int value) : order(order), value(value)
        {
        }
        ~Object()
        {
            order.push_back(value);
        }
        std::vector<int> &order;
        int               value;
    };
    std::vector<int> order;
    GraphBuildArena  arena(256);
    ASSERT_TRUE(std::holds_alternative<Object *>(arena.Make<Object>(order, 1)));
    auto second = arena.Make<Object>(order, 2);
    ASSERT_TRUE(std::holds_alternative<Object *>(second));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(std::get<Object *>(second)) % 64, 0u);
    auto                      borrower = arena.Borrow();
    std::weak_ptr<const void> weak     = borrower;
    EXPECT_TRUE(std::holds_alternative<FrameError>(arena.Reset()));
    EXPECT_TRUE(std::holds_alternative<FrameError>(arena.Make<int>(3)));
    EXPECT_TRUE(order.empty());
    borrower.reset();
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(arena.Reset()));
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(order, (std::vector<int>{2, 1}));
    auto sealed = arena.Seal();
    EXPECT_FALSE(arena.Seal());
    EXPECT_TRUE(std::holds_alternative<FrameError>(arena.Reset()));
    sealed.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(arena.Reset()));
    GraphBuildArena tiny(1);
    EXPECT_EQ(std::get<FrameError>(tiny.Make<Object>(order, 3)).code, FrameErrorCode::OUT_OF_MEMORY);
}

TEST(FrameBuildLifetime, BuilderCannotResetWhileFrozenGraphIsBorrowed)
{
    FrameGraphBuilder builder;
    auto              graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    const auto        id    = graph->GetId();
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Reset()));
    std::weak_ptr<const FrozenFrameGraph> weak = graph;
    graph.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Reset()));
    EXPECT_TRUE(weak.expired());
    graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    EXPECT_NE(id, graph->GetId());
}

TEST(FrameBuildLifetime, CacheEvictionPreservesBorrowersAndDoesNotCacheFailures)
{
    FrameObjectCache<uint32_t, std::shared_ptr<int>> cache(2);
    uint32_t                                         creates = 0;
    auto                                             acquire = [&](uint32_t key) {
        return cache.Acquire(key, [&]() -> std::variant<FrameError, std::shared_ptr<int>> {
            ++creates;
            return std::make_shared<int>(key);
        });
    };
    auto pinned = std::get<std::shared_ptr<int>>(acquire(1));
    EXPECT_EQ(std::get<std::shared_ptr<int>>(acquire(1)), pinned);
    auto               second     = std::get<std::shared_ptr<int>>(acquire(2));
    std::weak_ptr<int> weakSecond = second;
    second.reset();
    acquire(1);
    acquire(3);
    EXPECT_TRUE(weakSecond.expired());
    EXPECT_EQ(creates, 3u);
    cache.Clear();
    EXPECT_EQ(*pinned, 1);
    auto failed = cache.Acquire(4, []() -> std::variant<FrameError, std::shared_ptr<int>> { return FrameError{FrameErrorCode::COMPILE_FAILED}; });
    EXPECT_TRUE(std::holds_alternative<FrameError>(failed));
    EXPECT_EQ(*std::get<std::shared_ptr<int>>(acquire(4)), 4);
    EXPECT_EQ(creates, 4u);
}

TEST_P(CopyGraph, InvalidDeclarationsFailBeforeAllocationEvenWithoutExports)
{
    for (auto mode : {VulkanFrameGraphMode::DYNAMIC_RENDERING, VulkanFrameGraphMode::RENDER_PASS}) {
        if (GetParam() == API::DX12 && mode == VulkanFrameGraphMode::RENDER_PASS)
            continue;
        context.reset(device->CreateFrameContext({2, 1, 61, 2, mode}));
        ASSERT_TRUE(context);
        const auto lease = Begin();
        for (bool compute : {false, true}) {
            FrameGraphBuilder builder;
            if (compute)
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute({})));
            else
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Custom({"native callback"})));
            auto compiled = context->Compile(lease, Freeze(builder));
            ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
            EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::COMPILE_FAILED);
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_P(CopyGraph, SubmissionReleasesCpuGraphBeforeGpuRetirement)
{
    auto                                  graph    = SimpleGraph();
    std::weak_ptr<const FrozenFrameGraph> weak     = graph;
    const auto                            lease    = Begin();
    auto                                  compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    EXPECT_FALSE(weak.expired());
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(Collect(1).size(), 1u);
}

TEST_P(CopyGraph, FrozenGraphSurvivesBuilderAndInputsAcrossTwoSlots)
{
    for (uint32_t round = 0; round < 3; ++round) {
        auto products = PreparePayload(round + 13);
        ASSERT_TRUE(products);
        std::weak_ptr<const PrepareProducts>     weak = products;
        std::array<BufferPtr, 2>                 readbacks;
        std::array<std::array<std::byte, 64>, 2> expected;
        for (uint32_t slot = 0; slot < 2; ++slot) {
            std::shared_ptr<const FrozenFrameGraph> graph;
            {
                FrameGraphBuilder builder;
                auto              upload  = BufferOf(MemoryType::CPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC);
                auto              scratch = BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST);
                auto              dead    = BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_DST);
                readbacks[slot] =
                    BufferOf(GetParam() == API::VULKAN ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU, BufferUsageFlagBit::TRANSFER_DST);
                ASSERT_TRUE(upload && scratch && dead && readbacks[slot]);
                const auto bytes = products->Read(products->records[slot]);
                ASSERT_EQ(bytes.size(), 64u);
                std::memcpy(expected[slot].data(), bytes.data(), 64);
                auto *mapped = upload->Map();
                ASSERT_TRUE(mapped);
                std::memcpy(mapped, bytes.data(), 64);
                upload->UnMap();
                auto src    = Import(builder, upload, CopyBufferState::HOST_WRITE, true);
                auto mid    = Import(builder, scratch, CopyBufferState::COMMON, false);
                auto dst    = Import(builder, readbacks[slot], CopyBufferState::HOST_READ, false);
                auto unused = Import(builder, dead, CopyBufferState::COMMON, false);
                EXPECT_EQ(Import(builder, upload, CopyBufferState::HOST_WRITE, true), src);
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, mid, 0, 0, 32})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({mid, dst, 0, 0, 32})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, mid, 32, 0, 32})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({mid, dst, 0, 32, 32})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, unused, 0, 0, 64})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({dst, 0, 64})));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.KeepAlive(products)));
                graph = Freeze(builder);
                EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Copy({src, mid, 0, 0, 32})));
                EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Export({dst, 0, 64})));
                EXPECT_TRUE(
                    std::holds_alternative<FrameError>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true)));
                EXPECT_TRUE(std::holds_alternative<FrameError>(builder.KeepAlive({})));
                EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Freeze()));
            }
            ASSERT_TRUE(graph);
            const auto analysis = AnalyzeFrameGraph(*graph);
            ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
            EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, (std::vector<uint32_t>{0, 1, 2, 3}));
            auto lease    = Begin();
            auto compiled = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto       plan     = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            const auto identity = plan->GetIdentity();
            EXPECT_EQ(identity.graphId, graph->GetId());
            EXPECT_EQ(identity.lease, lease);
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, graph)));
            graph.reset();
            auto dispatched = context->Dispatch(plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
            EXPECT_FALSE(plan);
            const auto ticket = std::get<EncodingTicket>(dispatched);
            auto       wrong  = ticket;
            ++wrong.sessionId;
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(wrong)));
            auto collected = context->TryCollect(ticket);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
            auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
            EXPECT_EQ(recording->GetIdentity(), identity);
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        }
        products.reset();
        EXPECT_FALSE(weak.expired());
        EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
        const auto complete = Collect(2);
        ASSERT_EQ(complete.size(), 2u);
        for (const auto &item : complete)
            EXPECT_EQ(item.status, FrameCompletionStatus::COMPLETE);
        EXPECT_TRUE(weak.expired());
        for (size_t i = 0; i < readbacks.size(); ++i) {
            auto *mapped = readbacks[i]->Map();
            ASSERT_TRUE(mapped);
            EXPECT_EQ(std::memcmp(mapped, expected[i].data(), 64), 0);
            readbacks[i]->UnMap();
        }
    }
}

TEST_P(CopyGraph, RejectsUndefinedRangesOverflowAndForeignHandlesWithoutBorrowingSlot)
{
    for (uint32_t fault = 0; fault < 7; ++fault) {
        FrameGraphBuilder builder;
        auto              source =
            Import(builder, BufferOf(MemoryType::CPU_ONLY, fault == 5 ? BufferUsageFlagBit::TRANSFER_DST : BufferUsageFlagBit::TRANSFER_SRC),
                   CopyBufferState::HOST_WRITE, fault != 0);
        auto destination = Import(builder, BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_DST), CopyBufferState::COMMON, false);
        BufferCopyPass pass{source, destination, 0, 0, 64};
        if (fault == 1)
            pass.size = 32;
        if (fault == 2)
            pass.sourceOffset = UINT64_MAX;
        if (fault == 3)
            ++pass.source.graph;
        if (fault == 4)
            pass.destination = source;
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(pass)));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({destination, 0, 64})));
        auto lease = Begin();
        auto graph = Freeze(builder);
        if (fault == 6) {
            auto data            = graph->GetData();
            data.buffers[0].size = 128;
            graph                = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        }
        auto compiled = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
        EXPECT_EQ(std::get<FrameError>(compiled).code, fault < 5 ? FrameErrorCode::COMPILE_FAILED : FrameErrorCode::INVALID_ARGUMENT);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_P(CopyGraph, SideEffectKeepsPartialWritersButNotOverwrittenUndefinedProducer)
{
    FrameGraphBuilder builder;
    auto              upload = BufferOf(MemoryType::CPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC);
    auto readback            = BufferOf(GetParam() == API::VULKAN ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU, BufferUsageFlagBit::TRANSFER_DST);
    ASSERT_TRUE(upload && readback);
    std::array<uint8_t, 64> expected;
    for (uint32_t i = 0; i < expected.size(); ++i)
        expected[i] = static_cast<uint8_t>(i * 3 + 1);
    auto *mapped = upload->Map();
    ASSERT_TRUE(mapped);
    std::memcpy(mapped, expected.data(), expected.size());
    upload->UnMap();
    auto undefined    = Import(builder, BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC), CopyBufferState::COMMON, false);
    auto source       = Import(builder, upload, CopyBufferState::HOST_WRITE, true);
    auto intermediate = Import(builder, BufferOf(MemoryType::GPU_ONLY, BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST),
                               CopyBufferState::COMMON, false);
    auto destination  = Import(builder, readback, CopyBufferState::HOST_READ, false);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({undefined, intermediate, 0, 0, 64})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, intermediate, 0, 0, 32})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, intermediate, 32, 32, 32})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({intermediate, destination, 0, 0, 64, true})));
    auto graph    = Freeze(builder);
    auto analysis = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, (std::vector<uint32_t>{1, 2, 3}));
    auto compiled = context->Compile(Begin(), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    auto completed = Collect(1);
    ASSERT_EQ(completed.size(), 1u);
    ASSERT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
    mapped = readback->Map();
    ASSERT_TRUE(mapped);
    EXPECT_EQ(std::memcmp(mapped, expected.data(), expected.size()), 0);
    readback->UnMap();
}

TEST_P(CopyGraph, ImportsAreNotRootsAndEmptyGraphNeverNeedsSubmission)
{
    auto lease = Begin();
    auto graph = SimpleGraph(false);
    ASSERT_TRUE(graph);
    auto result = context->Compile(lease, graph);
    EXPECT_TRUE(std::holds_alternative<FrameNoWork>(result));
    std::array<FrameCompletion, 1> completion;
    EXPECT_EQ(context->PollCompleted(completion), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, graph)));
}

TEST_P(CopyGraph, CompiledPlanKeepsLeaseAndWrongContextCannotConsumeIt)
{
    auto lease  = Begin();
    auto graph  = SimpleGraph();
    auto result = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    auto                                plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(result));
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext({2, 1, 61}));
    ASSERT_TRUE(other);
    EXPECT_TRUE(std::holds_alternative<FrameError>(other->Dispatch(plan)));
    EXPECT_TRUE(plan);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    plan.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, graph)));
    auto next = Begin();
    result    = context->Compile(next, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    std::get<std::unique_ptr<BackendCompiledFrame>>(result).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
}

TEST_P(CopyGraph, CancelDrainsSerialRecordingBeforeDiscard)
{
    auto                                  lease  = Begin();
    auto                                  graph  = SimpleGraph();
    std::weak_ptr<const FrozenFrameGraph> weak   = graph;
    auto                                  result = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(result));
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_FALSE(weak.expired());
    const auto ticket = std::get<EncodingTicket>(dispatched);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(context->TryCollect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(Collect(0).empty());
}

TEST_P(CopyGraph, NativeBeginFailureProducesFailedEncodingWithoutSubmission)
{
    auto lease  = Begin();
    auto result = context->Compile(lease, SimpleGraph());
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    auto                  plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(result));
    std::function<void()> restoreFunction;
    Finally               restore{[&] { restoreFunction(); }};
    if (GetParam() == API::VULKAN) {
        auto      &fn           = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
        const auto saved        = fn.vkBeginCommandBuffer;
        restoreFunction         = [&fn, saved] { fn.vkBeginCommandBuffer = saved; };
        fn.vkBeginCommandBuffer = RejectGraphBegin;
    } else {
        auto      &fn    = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
        const auto saved = fn.resetList;
        restoreFunction  = [&fn, saved] { fn.resetList = saved; };
        fn.resetList     = [](ID3D12GraphicsCommandList *, ID3D12CommandAllocator *) -> HRESULT { return E_OUTOFMEMORY; };
    }
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_FALSE(plan);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(std::get<EncodingTicket>(dispatched))));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.cause.nativeCode,
              GetParam() == API::VULKAN ? static_cast<int64_t>(VK_ERROR_OUT_OF_HOST_MEMORY) : static_cast<int64_t>(E_OUTOFMEMORY));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    std::array<FrameCompletion, 1> completion;
    EXPECT_EQ(context->PollCompleted(completion), 0u);
}

TEST_P(CopyGraph, BackendRejectsForeignCompilerOptionAndInvalidImageGraph)
{
    DeviceFrameContextInitInfo info{1, 1, 62, 1};
    info.vulkanCompiler = static_cast<VulkanFrameGraphMode>(255);
    std::unique_ptr<DeviceFrameContext> invalid(device->CreateFrameContext(info));
    EXPECT_FALSE(invalid);
    if (GetParam() == API::DX12) {
        info.vulkanCompiler = VulkanFrameGraphMode::RENDER_PASS;
        invalid.reset(device->CreateFrameContext(info));
        EXPECT_FALSE(invalid);
        FrameGraphData data;
        data.images.emplace_back();
        const auto lease  = Begin();
        auto       result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(1, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, FrameErrorCode::COMPILE_FAILED);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

INSTANTIATE_TEST_SUITE_P(WindowsBackends, CopyGraph, testing::Values(API::VULKAN, API::DX12));

TEST(FrameGraphImports, BufferByteRangesNormalizeAndDeduplicatePhysicalImports)
{
    FrameGraphBuffer input{BufferPtr(new ContentBuffer()), 64};
    input.initializedRanges = {{16, 16}, {0, 8}, {8, 12}, {48, 16}};
    FrameGraphBuilder builder;
    const auto        first = builder.Import(input);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(first));
    EXPECT_EQ(std::get<std::vector<BufferContentRange>>(ResolveInitializedBufferContents(input)),
              (std::vector<BufferContentRange>{{0, 32}, {48, 16}}));
    auto equivalent              = input;
    equivalent.initializedRanges = {{48, 16}, {0, 32}};
    EXPECT_EQ(std::get<GraphBufferHandle>(builder.Import(equivalent)), std::get<GraphBufferHandle>(first));
    for (uint32_t fault = 0; fault < 4; ++fault) {
        auto conflict = input;
        if (fault == 0)
            conflict.initializedRanges.push_back({32, 16});
        else if (fault == 1)
            conflict.initializedRanges.pop_back();
        else if (fault == 2)
            conflict.final = CopyBufferState::COPY_SOURCE;
        else
            conflict.size = 32;
        EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(conflict)));
    }
    auto full              = input;
    full.initializedRanges = {{0, 32}, {32, 32}};
    FrameGraphBuilder other;
    const auto        whole = other.Import(full.buffer, full.initial, full.final, true);
    EXPECT_EQ(std::get<GraphBufferHandle>(other.Import(full)), std::get<GraphBufferHandle>(whole));
    const auto frozen = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    EXPECT_EQ(frozen->GetData().buffers.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(input)));
}

TEST(FrameGraphImports, BufferInitialRangesRejectOverflowContradictionsAndExcessDeclarations)
{
    FrameGraphBuffer input{BufferPtr(new ContentBuffer()), 64};
    for (uint32_t fault = 0; fault < 9; ++fault) {
        auto invalid              = input;
        invalid.initializedRanges = {{0, 4}};
        if (fault == 0)
            invalid.initializedRanges = {{0, 0}};
        else if (fault == 1)
            invalid.initializedRanges = {{64, 4}};
        else if (fault == 2)
            invalid.initializedRanges = {{4, UINT64_MAX}};
        else if (fault == 3)
            invalid.initializedRanges = {{UINT64_MAX, 8}};
        else if (fault == 4)
            invalid.initialized = true;
        else if (fault == 5)
            invalid.transient = Buffer::Descriptor{64, BufferUsageFlagBit::TRANSFER_DST};
        else if (fault == 6)
            invalid.initializedRanges.assign(MAX_BUFFER_INITIAL_RANGES + 1, {0, 4});
        else if (fault == 7)
            invalid.size = 0;
        else
            invalid.initial = static_cast<CopyBufferState>(255);
        EXPECT_TRUE(std::holds_alternative<FrameError>(ResolveInitializedBufferContents(invalid))) << fault;
        FrameGraphBuilder builder;
        EXPECT_TRUE(std::holds_alternative<FrameError>(builder.Import(invalid))) << fault;
        FrameGraphData data;
        data.buffers.push_back(invalid);
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data)))) << fault;
    }
    input.size              = UINT64_MAX;
    input.initializedRanges = {{UINT64_MAX - 3, 3}, {0, 1}};
    EXPECT_TRUE(std::holds_alternative<std::vector<BufferContentRange>>(ResolveInitializedBufferContents(input)));
    input.initializedRanges.assign(MAX_BUFFER_INITIAL_RANGES, {0, 1});
    EXPECT_EQ(std::get<std::vector<BufferContentRange>>(ResolveInitializedBufferContents(input)), (std::vector<BufferContentRange>{{0, 1}}));
}

TEST(FrameGraphImports, BufferByteHolesRejectCopyImageVertexIndexUniformAndExportReads)
{
    for (uint32_t kind = 0; kind < 6; ++kind) {
        FrameGraphData   data;
        FrameGraphBuffer input{BufferPtr(new ContentBuffer()), 64};
        input.initializedRanges = {{0, 16}, {17, 47}};
        data.buffers.push_back(input);
        data.buffers.push_back({BufferPtr(new ContentBuffer()), 64});
        FrameGraphImage target;
        target.image             = ImagePtr(new Image());
        target.descriptor.extent = {2, 2, 1};
        target.descriptor.format = PixelFormat::RGBA8_UNORM;
        target.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_DST;
        data.images.push_back(target);
        if (kind == 0) {
            data.passes.push_back(BufferCopyPass{{1, 0}, {1, 1}, 8, 0, 16, true});
        } else if (kind == 1) {
            BufferImageCopyPass copy;
            copy.buffer              = {1, 0};
            copy.image               = {1, 0};
            copy.region.bufferOffset = 8;
            copy.region.subRange     = {0, 0, 1, AspectFlagBit::COLOR_BIT};
            copy.region.imageExtent  = {2, 2, 1};
            copy.sideEffect          = true;
            data.passes.push_back(copy);
        } else if (kind == 5) {
            data.exports.push_back({{1, 0}, 0, 64});
        } else {
            RasterDraw draw;
            draw.shader  = ShaderPtr(new Shader());
            draw.command = {3, 1, 0, 0};
            if (kind == 2)
                draw.vertices.push_back({{{1, 0}, 0, 24}, 8});
            else if (kind == 3)
                draw.indexed = RasterIndexedDraw{{{1, 0}, 8, 16}, IndexType::U32, {3, 1, 0, 0, 0}};
            else
                draw.uniforms.push_back({0, 0, {{1, 0}, 0, 16}, 8});
            FrameRasterPass raster;
            raster.color      = {1, 0};
            raster.sideEffect = true;
            raster.draws.push_back(draw);
            data.passes.push_back(raster);
        }
        const auto rejected = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
        ASSERT_TRUE(std::holds_alternative<FrameError>(rejected)) << kind;
        EXPECT_NE(std::get<FrameError>(rejected).message.find("undefined contents"), std::string::npos) << kind;
        data.buffers.front().initializedRanges.push_back({16, 1});
        EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(FrozenFrameGraph(1, data)))) << kind;
    }
}

TEST(FrameGraphImports, PartialByteImportsDoNotKeepDeadWritersOrDefineTransientAliases)
{
    FrameGraphData data;
    data.buffers.push_back({BufferPtr(new ContentBuffer()), 64});
    data.buffers.back().initializedRanges = {{0, 32}};
    data.buffers.push_back({BufferPtr(new ContentBuffer()), 64});
    data.buffers.push_back({BufferPtr(new ContentBuffer()), 64});
    data.buffers.push_back({BufferPtr(new ContentBuffer()), 64, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true});
    data.passes.push_back(BufferCopyPass{{1, 1}, {1, 0}, 0, 32, 32});
    data.passes.push_back(BufferCopyPass{{1, 3}, {1, 0}, 32, 32, 32});
    data.passes.push_back(BufferCopyPass{{1, 0}, {1, 2}, 0, 0, 64, true});
    const auto analysis = AnalyzeFrameGraph(FrozenFrameGraph(1, data));
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, (std::vector<uint32_t>{1, 2}));
    data.buffers.front().buffer    = {};
    data.buffers.front().transient = Buffer::Descriptor{64, BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST};
    EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(FrozenFrameGraph(1, data))));
}
