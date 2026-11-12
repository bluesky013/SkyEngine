//
// Created on 2026/10/27.
//

#include "AuroraTestHelper.h"
#include "RendererRuntime.h"
#include "graph/FrameComposer.h"
#include "resource/ResourceCatalog.h"

#include <array>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/renderer/PassResourceBindings.h>
#include <aurora/renderer/RendererProvider.h>
#include <deque>

using namespace sky;
using namespace sky::aurora;

namespace {
    struct CpuCompletionQueueProbe {
        size_t PollEvents(std::span<CpuCoordinatorEvent> destination)
        {
            ++polls;
            if (destination.empty() || events.empty())
                return 0;
            destination[0] = std::move(events.front());
            events.pop_front();
            return 1;
        }
        std::deque<CpuCoordinatorEvent> events;
        size_t                          polls = 0;
    };
    struct RendererCompletionSinkProbe {
        void Push(RendererEvent event)
        {
            events.push_back(std::move(event));
        }
        std::deque<RendererEvent> events;
        bool                      failed = false;
    };
} // namespace

TEST(RendererRuntimeEvents, SuccessfulCpuCompletionsCannotHideStoppedFailure)
{
    CpuCompletionQueueProbe cpu;
    for (const auto sequence : {1U, 2U}) {
        CpuPacketCompleted completed;
        completed.application.sequence = sequence;
        completed.preparationAttempted = true;
        ASSERT_TRUE(completed.preparation.Succeeded());
        cpu.events.push_back(std::move(completed));
    }
    cpu.events.push_back(CpuCoordinatorStopped{RendererError{RendererErrorCode::DEVICE_LOST, "injected terminal failure"}});
    RendererCompletionSinkProbe sink;
    sky::aurora::detail::DrainRuntimeCompletions(cpu, sink);
    ASSERT_EQ(sink.events.size(), 2U);
    ASSERT_TRUE(std::holds_alternative<RendererFailure>(sink.events[0]));
    const auto &failure = std::get<RendererFailure>(sink.events[0]);
    EXPECT_EQ(failure.packetSequence, 0U);
    EXPECT_EQ(failure.error.code, RendererErrorCode::DEVICE_LOST);
    EXPECT_EQ(failure.error.message, "injected terminal failure");
    EXPECT_TRUE(std::holds_alternative<RendererStopped>(sink.events[1]));
    EXPECT_TRUE(sink.failed);
    EXPECT_EQ(cpu.polls, 3U);
    EXPECT_TRUE(cpu.events.empty());
}

TEST(RendererRuntimeEvents, PacketFailureKeepsItsPriorityAndLaterStoppedRemainsDeliverable)
{
    CpuCompletionQueueProbe cpu;
    CpuPacketCompleted      completed;
    completed.application.sequence = 7;
    completed.application.error    = RendererError{RendererErrorCode::INVALID_PACKET, "packet failure"};
    completed.executionError       = RendererError{RendererErrorCode::DEVICE_LOST, "later failure"};
    cpu.events.push_back(std::move(completed));
    cpu.events.push_back(CpuCoordinatorStopped{});
    RendererCompletionSinkProbe sink;
    sky::aurora::detail::DrainRuntimeCompletions(cpu, sink);
    ASSERT_EQ(sink.events.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<RendererFailure>(sink.events[0]));
    EXPECT_EQ(std::get<RendererFailure>(sink.events[0]).packetSequence, 7U);
    EXPECT_EQ(std::get<RendererFailure>(sink.events[0]).error.code, RendererErrorCode::INVALID_PACKET);
    EXPECT_FALSE(sink.failed);
    sink.events.clear();
    sky::aurora::detail::DrainRuntimeCompletions(cpu, sink);
    ASSERT_EQ(sink.events.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<RendererStopped>(sink.events[0]));
    EXPECT_TRUE(cpu.events.empty());
}

TEST(RendererRuntimeEvents, EmptyCpuQueueDoesNotInventAPublicEvent)
{
    CpuCompletionQueueProbe     cpu;
    RendererCompletionSinkProbe sink;
    sky::aurora::detail::DrainRuntimeCompletions(cpu, sink);
    EXPECT_TRUE(sink.events.empty());
    EXPECT_FALSE(sink.failed);
    EXPECT_EQ(cpu.polls, 1U);
}

TEST(RendererRuntimeEvents, RealStoppedCoordinatorDrainsSuccessResultsBeforePublishingStopped)
{
    CpuRenderCoordinator cpu;
    ASSERT_FALSE(cpu.Init({2, 3}));
    for (const auto sequence : {1U, 2U}) {
        auto packet      = std::make_unique<FramePacket>();
        packet->sequence = sequence;
        ASSERT_EQ(cpu.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
    }
    cpu.RequestShutdown();
    cpu.Join();
    ASSERT_EQ(cpu.GetState(), CpuCoordinatorState::STOPPED);
    RendererCompletionSinkProbe sink;
    sky::aurora::detail::DrainRuntimeCompletions(cpu, sink);
    ASSERT_EQ(sink.events.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<RendererStopped>(sink.events[0]));
    EXPECT_FALSE(sink.failed);
    std::array<CpuCoordinatorEvent, 1> remaining;
    EXPECT_EQ(cpu.PollEvents(remaining), 0U);
}

TEST(RendererConfiguration, UnknownBackendsAreRejectedBeforeBackendAcquisition)
{
    auto *originalDevice = Instance::Get()->GetDevice();
    auto  provider       = CreateRendererProvider();
    for (const auto value : {-1, 255, INT32_MAX}) {
        RendererInitInfo info;
        info.backend  = static_cast<API>(value);
        auto provided = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(provided));
        EXPECT_EQ(std::get<RendererError>(provided).code, RendererErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(Instance::Get()->GetDevice(), originalDevice);
    }
}

namespace {
    class CompositionShader final : public Shader {
    public:
        CompositionShader(Device *owner, ShaderReflection source) : owner(owner), reflection(std::move(source))
        {
        }
        const Device *GetDevice() const noexcept override
        {
            return owner;
        }
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }
        bool InitializeNames()
        {
            return BuildShaderParameterTable(reflection, 123, {}, parameters, error);
        }
        Device          *owner;
        ShaderReflection reflection;
        std::string      error;
    };
    ShaderReflection UniformReflection()
    {
        ShaderResource resource;
        resource.name             = "Pass";
        resource.type             = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        resource.set              = 1;
        resource.stages           = ShaderStageFlagBit::FS;
        resource.declarationOwner = "test/framegraph-composition";
        ShaderBlockMember field{
            .name = "gain", .offset = 0, .size = 4, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::SCALAR, .alignment = 4};
        ShaderReflection reflection;
        reflection.resources      = {resource};
        reflection.declarations   = reflection.resources;
        reflection.blocks         = {{"Pass", "PassData", 1, 0, 16, {field}, 16}};
        reflection.declaredBlocks = reflection.blocks;
        reflection.kind           = ShaderReflectionKind::PROGRAM;
        reflection.complete       = true;
        reflection.stages         = ShaderStageFlagBit::FS;
        reflection.bindingModel   = ShaderBindingModel::VULKAN_DESCRIPTORS;
        reflection.nativeBindings = {{1, 0, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::DESCRIPTOR, 1, 1}};
        return reflection;
    }
    class FrameGraphComposition : public testing::Test {
    protected:
        void SetUp() override
        {
#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
            constexpr auto api = API::METAL;
#else
            constexpr auto api = API::VULKAN;
#endif
            Instance::Get()->Init({"FrameGraphComposition", "SkyEngine", true, api});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            DeviceFrameContextInitInfo info;
            info.allocations.uniformBytes = device->GetCapability().minUniformBufferOffsetAlignment;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            const auto begun = context->TryBeginSlot();
            ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
            inputs.lease = std::get<DeviceFrameLease>(begun);
        }
        void TearDown() override
        {
            context.reset();
            Instance::Destroy();
        }
        Device                             *device = nullptr;
        std::unique_ptr<DeviceFrameContext> context;
        FrameBuildInputs                    inputs;
    };
} // namespace

TEST_F(FrameGraphComposition, SharedParameterGroupsDoNotExhaustTheUniformBudget)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    auto              reflection     = UniformReflection();
    auto             *implementation = new CompositionShader(device, reflection);
    ShaderPtr         shader(implementation);
    ASSERT_TRUE(implementation->InitializeNames()) << implementation->error;
    PassResourceBindings pass(reflection);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(pass.Data(Name("Pass.gain"), MakeShaderData(0.25f))));
    const PassResourceBindings *groups[]{&pass};
    RasterDraw                  first, second;
    first.shader = second.shader = shader;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(composer, first, groups)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(composer, second, groups)));
    EXPECT_EQ(first.groups[1].group, second.groups[1].group);
    const auto firstBindings  = ResolveRasterBindings(first);
    const auto secondBindings = ResolveRasterBindings(second);
    ASSERT_EQ(firstBindings.uniforms.size(), 1u);
    ASSERT_EQ(secondBindings.uniforms.size(), 1u);
    EXPECT_EQ(firstBindings.uniforms[0].dynamicOffset, secondBindings.uniforms[0].dynamicOffset);

    PassResourceBindings changed(reflection);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(changed.Data(Name("Pass.gain"), MakeShaderData(0.75f))));
    const PassResourceBindings *changedGroups[]{&changed};
    const auto                  previous = second.groups[1].group;
    const auto                  failed   = PassResourceBindings::Apply(composer, second, changedGroups);
    ASSERT_TRUE(std::holds_alternative<FrameError>(failed));
    EXPECT_EQ(std::get<FrameError>(failed).code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(second.groups[1].group, previous);

    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(inputs.lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(PassResourceBindings::Apply(composer, second, groups)));
}

TEST_F(FrameGraphComposition, ShaderLayoutIdentityPreventsParameterGroupReuse)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    const auto        reflection   = UniformReflection();
    auto             *firstShader  = new CompositionShader(device, reflection);
    auto             *secondShader = new CompositionShader(device, reflection);
    ShaderPtr         firstOwner(firstShader), secondOwner(secondShader);
    ASSERT_TRUE(firstShader->InitializeNames());
    ASSERT_TRUE(secondShader->InitializeNames());
    ShaderParameterTransactionState state;
    state.fields[0]["gain"] = MakeShaderData(0.5f);
    const auto first        = composer.MaterializeParameters(firstOwner, 1, state, {});
    const auto second       = composer.MaterializeParameters(secondOwner, 1, state, {});
    ASSERT_TRUE(std::holds_alternative<FrameBindingGroupUse>(first));
    ASSERT_TRUE(std::holds_alternative<FrameBindingGroupUse>(second));
    EXPECT_NE(std::get<FrameBindingGroupUse>(first).group, std::get<FrameBindingGroupUse>(second).group);
    EXPECT_EQ(std::get<FrameBindingGroupUse>(first).group->GetData().layoutOwner.Get(), firstShader);
    EXPECT_EQ(std::get<FrameBindingGroupUse>(second).group->GetData().layoutOwner.Get(), secondShader);
}

TEST_F(FrameGraphComposition, FeatureCanDeclareGpuOnlyComputeStorageWithoutAnUpload)
{
    FrameGraphBuilder       builder;
    FrameComposer           composer(*device, *context, builder, inputs);
    FrameGraphDeclarations &graph = composer;
    Buffer::Descriptor      descriptor;
    descriptor.size    = 32;
    descriptor.memory  = MemoryType::GPU_ONLY;
    descriptor.usage   = BufferUsageFlagBit::STORAGE;
    const auto created = graph.CreateBuffer(descriptor);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(created));
    const auto       buffer = std::get<GraphBufferHandle>(created);
    ShaderReflection reflection;
    ShaderResource   output;
    output.type          = ShaderResourceType::STORAGE_BUFFER;
    output.bufferKind    = ShaderBufferKind::STRUCTURED;
    output.elementStride = 4;
    output.access        = ShaderResourceAccess::READ_WRITE;
    output.stages        = ShaderStageFlagBit::CS;
    reflection.resources = {output};
    FrameComputePass compute;
    compute.shader = new CompositionShader(device, reflection);
    ComputeBindingDraft bindings;
    bindings.buffers.push_back({0, 0, {buffer, 0, 32}, 0, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(compute, std::move(bindings))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(graph.Compute(std::move(compute))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(graph.Export(GraphBufferExport{buffer, 0, 32})));
    const auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    const auto &data = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen)->GetData();
    ASSERT_EQ(data.buffers.size(), 1u);
    EXPECT_TRUE(data.buffers[0].transient);
    EXPECT_FALSE(data.buffers[0].buffer);
    ASSERT_EQ(data.passes.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<FrameComputePass>(data.passes[0]));
    EXPECT_TRUE(std::holds_alternative<FrameGraphAnalysis>(AnalyzeFrameGraph(*std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen))));
}

TEST_F(FrameGraphComposition, CachedExternalUniformCanReturnToTheDefaultDynamicOffset)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    auto             *implementation = new CompositionShader(device, UniformReflection());
    ShaderPtr         shader(implementation);
    ASSERT_TRUE(implementation->InitializeNames());
    ShaderParameterTransactionState state;
    state.resources[{0, 0}] = {0, 0, 0, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC};
    RasterBindingDraft resources;
    resources.uniforms = {{1, 0, {{7, 0}, 0, 16}, 256}};
    const auto first   = composer.MaterializeParameters(shader, 1, state, resources);
    ASSERT_TRUE(std::holds_alternative<FrameBindingGroupUse>(first));
    resources.uniforms.front().dynamicOffset = 0;
    const auto second                        = composer.MaterializeParameters(shader, 1, state, resources);
    ASSERT_TRUE(std::holds_alternative<FrameBindingGroupUse>(second));
    EXPECT_EQ(std::get<FrameBindingGroupUse>(first).group, std::get<FrameBindingGroupUse>(second).group);
    EXPECT_TRUE(std::get<FrameBindingGroupUse>(second).bufferUses.empty());
    EXPECT_EQ(std::get<FrameBindingGroupUse>(first).bufferUses.front().dynamicOffset, 256u);
}

TEST_F(FrameGraphComposition, MismatchedParameterResourcesFailBeforeUniformAllocation)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    auto              reflection = UniformReflection();
    ShaderResource    image;
    image.name             = "Input";
    image.set              = 1;
    image.binding          = 1;
    image.type             = ShaderResourceType::SAMPLED_IMAGE;
    image.texture          = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4};
    image.stages           = ShaderStageFlagBit::FS;
    image.declarationOwner = "test/framegraph-composition";
    reflection.resources.push_back(image);
    reflection.declarations.push_back(image);
    reflection.nativeBindings.push_back({1, 1, ShaderStageFlagBit::FS, 1, ShaderNativeCategory::DESCRIPTOR, 1, 1});
    auto     *implementation = new CompositionShader(device, reflection);
    ShaderPtr shader(implementation);
    ASSERT_TRUE(implementation->InitializeNames()) << implementation->error;
    ShaderParameterTransactionState state;
    state.fields[0]["gain"] = MakeShaderData(0.5f);
    state.resources[{1, 0}] = {1, 0, 0, ShaderResourceType::SAMPLED_IMAGE};
    const auto before       = context->AvailableFrameBufferBytes(inputs.lease, FrameBufferKind::UNIFORM);
    ASSERT_TRUE(std::holds_alternative<uint64_t>(before));
    RasterBindingDraft resources;
    resources.images    = {{1, 2, {7, 0}, {}}};
    const auto rejected = composer.MaterializeParameters(shader, 1, state, resources);
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::INVALID_ARGUMENT);
    const auto after = context->AvailableFrameBufferBytes(inputs.lease, FrameBufferKind::UNIFORM);
    ASSERT_TRUE(std::holds_alternative<uint64_t>(after));
    EXPECT_EQ(std::get<uint64_t>(before), std::get<uint64_t>(after));

    resources.images.front().binding = 1;
    ASSERT_TRUE(std::holds_alternative<FrameBindingGroupUse>(composer.MaterializeParameters(shader, 1, state, resources)));
    state.fields[31]["unused"] = MakeShaderData(0.5f);
    const auto unused          = composer.MaterializeParameters(shader, 1, state, resources);
    ASSERT_TRUE(std::holds_alternative<FrameError>(unused));
    EXPECT_EQ(std::get<FrameError>(unused).code, FrameErrorCode::INVALID_ARGUMENT);
}

TEST_F(FrameGraphComposition, PersistentBufferImportKeepsOwnershipAndRejectsConflicts)
{
    FrameGraphBuilder  builder;
    FrameComposer      composer(*device, *context, builder, inputs);
    Buffer::Descriptor descriptor;
    descriptor.size   = 64;
    descriptor.memory = MemoryType::GPU_ONLY;
    descriptor.usage  = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_DST;
    BufferPtr persistent(device->CreateBuffer(descriptor));
    ASSERT_TRUE(persistent);
    BufferPtr alias = persistent;

    const auto imported = composer.ImportBuffer(persistent, CopyBufferState::COMMON, CopyBufferState::COMMON, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
    const auto handle = std::get<GraphBufferHandle>(imported);
    persistent        = nullptr;

    const auto duplicate = composer.ImportBuffer(alias, CopyBufferState::COMMON, CopyBufferState::COMMON, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(duplicate));
    EXPECT_EQ(std::get<GraphBufferHandle>(duplicate).index, handle.index);

    const auto conflict = composer.ImportBuffer(alias, CopyBufferState::COMMON, CopyBufferState::COMMON, true);
    ASSERT_TRUE(std::holds_alternative<FrameError>(conflict));
    EXPECT_EQ(std::get<FrameError>(conflict).code, FrameErrorCode::INVALID_ARGUMENT);

    const auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    const auto &data = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen)->GetData();
    ASSERT_EQ(data.buffers.size(), 1u);
    EXPECT_TRUE(data.buffers[0].buffer);
    EXPECT_FALSE(data.buffers[0].transient);
}

TEST_F(FrameGraphComposition, OutputExportsAreExplicitAndRepeatedPublicationKeepsTheFinalContents)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    Image::Descriptor descriptor;
    descriptor.extent  = {1, 1, 1};
    descriptor.format  = PixelFormat::RGBA8_UNORM;
    descriptor.usage   = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
    const auto created = composer.CreateImage(descriptor);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(created));
    const auto       image = std::get<GraphImageHandle>(created);
    const ViewportId viewport{1, 1};
    composer.AddOutput({{viewport, 0, {1, 1}, descriptor.format}, image, 7}, true);
    FrameRasterPass first;
    first.color = image;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Raster(first)));
    EXPECT_FALSE(composer.GetOutput(viewport)->exportRequested);
    EXPECT_FALSE(composer.ResolveViewOutput({viewport, 7}));
    for (uint32_t i = 0; i < 300; ++i)
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Export(image)));
    EXPECT_TRUE(composer.GetOutput(viewport)->exportRequested);
    EXPECT_EQ(composer.ResolveViewOutput({viewport, 7}), image);
    EXPECT_FALSE(composer.ResolveViewOutput({viewport, 8}));
    EXPECT_FALSE(composer.ResolveViewOutput({{1, 2}, 7}));
    FrameRasterPass second;
    second.color = image;
    second.load  = composer.GetOutput(viewport)->exportRequested ? LoadOp::LOAD : LoadOp::CLEAR;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Raster(second)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Export(image)));
    const auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    const auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen);
    EXPECT_EQ(graph->GetData().imageExports.size(), 1u);
    const auto analyzed = AnalyzeFrameGraph(*graph);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses, (std::vector<uint32_t>{0, 1}));
    EXPECT_TRUE(std::holds_alternative<FrameError>(composer.Export(image)));
}

TEST_F(FrameGraphComposition, CopyAndMultisampleResolveCanPublishOutputWithoutADirectRasterWrite)
{
    for (const bool resolve : {false, true}) {
        SCOPED_TRACE(resolve);
        FrameGraphBuilder builder;
        FrameComposer     composer(*device, *context, builder, inputs);
        Image::Descriptor descriptor;
        descriptor.extent = {1, 1, 1};
        descriptor.format = PixelFormat::RGBA8_UNORM;
        descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_DST;
        const auto target = composer.CreateImage(descriptor);
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(target));
        const auto       image = std::get<GraphImageHandle>(target);
        const ViewportId viewport{1, 1};
        composer.AddOutput({{viewport, 0, {1, 1}, descriptor.format}, image}, false);
        if (resolve) {
            descriptor.samples = SampleCount::X4;
            descriptor.usage   = ImageUsageFlagBit::RENDER_TARGET;
            const auto source  = composer.CreateImage(descriptor);
            ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(source));
            FrameRasterPass pass;
            pass.color        = std::get<GraphImageHandle>(source);
            pass.store        = StoreOp::DONT_CARE;
            pass.colorResolve = RasterColorResolve{image};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Raster(std::move(pass))));
        } else {
            const std::array<std::byte, 4> bytes{};
            const auto                     upload = composer.Upload(bytes, BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::TRANSFER_SRC);
            ASSERT_TRUE(std::holds_alternative<RasterBufferRange>(upload));
            const auto          source = std::get<RasterBufferRange>(upload);
            BufferImageCopyPass copy;
            copy.buffer              = source.buffer;
            copy.image               = image;
            copy.region.bufferOffset = source.offset;
            copy.region.subRange     = {0, 0, 1, AspectFlagBit::COLOR_BIT};
            copy.region.imageExtent  = {1, 1, 1};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Copy(copy)));
        }
        EXPECT_FALSE(composer.GetOutput(viewport)->exportRequested);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Export(image)));
        EXPECT_TRUE(composer.GetOutput(viewport)->exportRequested);
        const auto frozen = builder.Freeze();
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
        const auto graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen);
        const auto analyzed = AnalyzeFrameGraph(*graph);
        ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analyzed));
        EXPECT_EQ(std::get<FrameGraphAnalysis>(analyzed).livePasses.size(), resolve ? 1u : 2u);
        const auto compiled = context->Compile(inputs.lease, graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled))
            << (std::holds_alternative<FrameError>(compiled) ? std::get<FrameError>(compiled).message : "No compiled output");
    }
}

TEST_F(FrameGraphComposition, ExportIntentDoesNotValidateUndefinedDiscardedOrPrematurelyReadContents)
{
    for (uint32_t fault = 0; fault < 4; ++fault) {
        SCOPED_TRACE(fault);
        FrameGraphBuilder builder;
        FrameComposer     composer(*device, *context, builder, inputs);
        Image::Descriptor descriptor;
        descriptor.extent  = {2, 2, 1};
        descriptor.format  = PixelFormat::RGBA8_UNORM;
        descriptor.usage   = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_DST;
        const auto created = composer.CreateImage(descriptor);
        ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(created));
        const auto       image = std::get<GraphImageHandle>(created);
        const ViewportId viewport{1, 1};
        composer.AddOutput({{viewport, 0, {2, 2}, descriptor.format}, image}, true);
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Export(image)));
        EXPECT_TRUE(composer.GetOutput(viewport)->exportRequested);
        if (fault == 1 || fault == 2) {
            FrameRasterPass pass;
            pass.color = image;
            if (fault == 1)
                pass.store = StoreOp::DONT_CARE;
            else {
                pass.load       = LoadOp::LOAD;
                pass.sideEffect = true;
            }
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Raster(pass)));
            if (fault == 2) {
                pass.load       = LoadOp::CLEAR;
                pass.sideEffect = false;
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Raster(pass)));
            }
        } else if (fault == 3) {
            const std::array<std::byte, 4> bytes{};
            const auto                     upload = composer.Upload(bytes, BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::TRANSFER_SRC);
            ASSERT_TRUE(std::holds_alternative<RasterBufferRange>(upload));
            const auto          source = std::get<RasterBufferRange>(upload);
            BufferImageCopyPass copy;
            copy.buffer              = source.buffer;
            copy.image               = image;
            copy.region.bufferOffset = source.offset;
            copy.region.subRange     = {0, 0, 1, AspectFlagBit::COLOR_BIT};
            copy.region.imageExtent  = {1, 1, 1};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(composer.Copy(copy)));
        }
        const auto frozen = builder.Freeze();
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
        const auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen);
        EXPECT_TRUE(std::holds_alternative<FrameError>(AnalyzeFrameGraph(*graph)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(inputs.lease, graph)));
    }
}

TEST_F(FrameGraphComposition, ImportedPreviousContentsDoNotPublishTheCurrentOutput)
{
    FrameGraphBuilder builder;
    FrameComposer     composer(*device, *context, builder, inputs);
    FrameGraphImage   imported;
    imported.descriptor.extent = {1, 1, 1};
    imported.descriptor.format = PixelFormat::RGBA8_UNORM;
    imported.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
    imported.image             = ImagePtr(device->CreateImage(imported.descriptor));
    ASSERT_TRUE(imported.image);
    imported.initial = imported.final = ImageLayout::GENERAL;
    imported.initialized              = true;
    const auto created                = builder.Import(std::move(imported));
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(created));
    const auto       image = std::get<GraphImageHandle>(created);
    const ViewportId viewport{1, 1};
    composer.AddOutput({{viewport, 0, {1, 1}, PixelFormat::RGBA8_UNORM}, image, 1}, true);
    EXPECT_FALSE(composer.GetOutput(viewport)->exportRequested);
    EXPECT_FALSE(composer.ResolveViewOutput({viewport, 1}));
    const auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    const auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen);
    EXPECT_TRUE(graph->GetData().images.front().initialized);
    EXPECT_TRUE(graph->GetData().imageExports.empty());
}

namespace {
    class PreparedRasterResolution : public FrameGraphComposition {
    protected:
        void SetUp() override
        {
            FrameGraphComposition::SetUp();
            if (!device || !context)
                return;
            const auto epoch = inputs.lease.context.deviceEpoch;
            meshKey          = {Uuid::CreateWithSeed(4201), 1, 1, epoch};
            materialKey      = {Uuid::CreateWithSeed(4202), 1, 1, epoch};
            imageKey         = {Uuid::CreateWithSeed(4203), 1, 1, epoch};
            Buffer::Descriptor descriptor;
            descriptor.size  = 16;
            descriptor.usage = BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX;
            BufferPtr buffer(device->CreateBuffer(descriptor));
            ASSERT_TRUE(buffer);
            auto meshCpu          = std::make_shared<ResourceVersionData>();
            meshCpu->resource     = meshKey;
            auto materialCpu      = std::make_shared<ResourceVersionData>();
            materialCpu->resource = materialKey;
            catalog.pins          = {std::make_shared<ResourceVersionLease>(
                                ResourceVersionLease{meshCpu, std::make_shared<const ResourceBacking>(ResourceBufferBacking{buffer})}),
                                     std::make_shared<ResourceVersionLease>(
                                ResourceVersionLease{materialCpu, std::make_shared<const ResourceBacking>(ResourceBufferBacking{})})};
            meshes.push_back({meshKey, {{0, 12, 12}}, {{0, 0, 0, Format::F_RGB32, "POSITION", 0}}, {{12, 4, IndexType::U16, {1, 1, 0, 0, 0}}}});
            view.view.scene = {1, 1};
            PreparedDrawPacket packet;
            packet.entity.scene      = view.view.scene;
            packet.entity.entity     = {1, 1};
            packet.mesh.resource     = meshKey;
            packet.material.resource = materialKey;
            view.draws.push_back(packet);
        }
        void TearDown() override
        {
            builder.Reset();
            catalog.pins.clear();
            FrameGraphComposition::TearDown();
        }
        PreparedMaterialLayout Layout(bool declaresImage, bool activeImage, bool requiresPosition = false)
        {
            ShaderReflection reflection;
            reflection.kind         = ShaderReflectionKind::PROGRAM;
            reflection.complete     = true;
            reflection.stages       = ShaderStageFlagBit::FS;
            reflection.bindingModel = ShaderBindingModel::VULKAN_DESCRIPTORS;
            if (requiresPosition) {
                reflection.stages |= ShaderStageFlagBit::VS;
                reflection.vertexInputs = {{"POSITION", 0, 0, ShaderScalarType::FLOAT, 3}};
            }
            if (declaresImage) {
                ShaderResource image{"opacity", ShaderResourceType::SAMPLED_IMAGE, 2, 1, 1};
                image.texture           = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
                image.stages            = ShaderStageFlagBit::FS;
                image.declarationOwner  = "test/prepared-raster";
                reflection.declarations = {image};
                if (activeImage) {
                    reflection.resources      = {image};
                    reflection.nativeBindings = {{2, 1, ShaderStageFlagBit::FS, 1, ShaderNativeCategory::DESCRIPTOR, 2, 1}};
                }
            }
            auto                  *shader = new CompositionShader(device, std::move(reflection));
            PreparedMaterialLayout layout;
            layout.resource   = materialKey;
            layout.shader     = ShaderPtr(shader);
            layout.vertexMask = RenderVertexMask(meshes.front().streams, meshes.front().attributes).mask;
            EXPECT_TRUE(shader->InitializeNames()) << shader->error;
            if (declaresImage)
                layout.parameters.images.push_back({Name("opacity"), imageKey});
            return layout;
        }
        std::variant<FrameError, PreparedRasterView>
        Resolve(const PreparedRasterIndex &index, Name tag = Name("opaque"), const ShaderVariant &pipeline = {})
        {
            return ResolvePreparedRaster(*device, *context, inputs.lease, builder, view.view.scene, view.draws, {}, index, tag, {}, pipeline);
        }
        RenderResourceKey               meshKey, materialKey, imageKey;
        FrozenResourceCatalog           catalog;
        std::vector<PreparedMeshLayout> meshes;
        PreparedViewData                view;
        FrameGraphBuilder               builder;
    };
} // namespace

TEST_F(PreparedRasterResolution, UnselectedTechniqueResourcesDoNotSuppressTheSelectedQueue)
{
    const std::array materials{
        PreparedMaterialTechniques{materialKey, {{Name("opaque"), Layout(false, false)}, {Name("shadow"), Layout(true, true)}}}};
    const auto indexed = BuildPreparedRasterIndex(catalog, meshes, materials);
    ASSERT_TRUE(std::holds_alternative<PreparedRasterIndex>(indexed));
    const auto &index = std::get<PreparedRasterIndex>(indexed);
    EXPECT_TRUE(index.GeometryReady(view.draws.front()));
    auto opaque = Resolve(index);
    ASSERT_TRUE(std::holds_alternative<PreparedRasterView>(opaque));
    EXPECT_EQ(std::get<PreparedRasterView>(opaque).draws.size(), 1u);
    EXPECT_EQ(std::get<PreparedRasterView>(opaque).pins.size(), 2u);
    for (const auto tag : {Name("shadow"), Name("other")}) {
        const auto omitted = Resolve(index, tag);
        ASSERT_TRUE(std::holds_alternative<PreparedRasterView>(omitted));
        EXPECT_TRUE(std::get<PreparedRasterView>(omitted).draws.empty());
    }
    catalog.pins.clear();
    EXPECT_EQ(std::get<PreparedRasterView>(opaque).pins.front()->cpu->resource, meshKey);
}

TEST_F(PreparedRasterResolution, InactiveImageParametersDoNotRequireBacking)
{
    const std::array materials{PreparedMaterialTechniques{materialKey, {{Name("opaque"), Layout(true, false)}}}};
    const auto       indexed = BuildPreparedRasterIndex(catalog, meshes, materials);
    ASSERT_TRUE(std::holds_alternative<PreparedRasterIndex>(indexed));
    const auto resolved = Resolve(std::get<PreparedRasterIndex>(indexed));
    ASSERT_TRUE(std::holds_alternative<PreparedRasterView>(resolved));
    EXPECT_EQ(std::get<PreparedRasterView>(resolved).draws.size(), 1u);
}

TEST_F(PreparedRasterResolution, UnavailableImagesDoNotHideInvalidVariantsAndResources)
{
    for (uint32_t fault = 0; fault < 6; ++fault) {
        SCOPED_TRACE(fault);
        auto       layout        = Layout(true, true);
        const auto originalCount = catalog.pins.size();
        if (fault == 0)
            layout.pipeline.entries.push_back({Name("SHADOWS"), 1});
        else if (fault == 1)
            ++layout.parameters.images.front().resource.deviceEpoch;
        else if (fault == 2)
            layout.parameters.images.front().name = Name("unknown");
        else if (fault >= 3) {
            auto cpu      = std::make_shared<ResourceVersionData>();
            cpu->resource = imageKey;
            if (fault == 3)
                catalog.pins.push_back(std::make_shared<ResourceVersionLease>(
                    ResourceVersionLease{cpu, std::make_shared<const ResourceBacking>(ResourceBufferBacking{})}));
            else {
                ResourceImageBacking backing;
                backing.descriptor.extent = {1, 1, 1};
                backing.descriptor.format = PixelFormat::RGBA8_UNORM;
                backing.descriptor.usage  = ImageUsageFlagBit::SAMPLED;
                backing.image             = fault == 4 ? ImagePtr(new Image()) : ImagePtr(device->CreateImage(backing.descriptor));
                ASSERT_TRUE(backing.image);
                if (fault == 5)
                    backing.descriptor.arrayLayers = 2;
                catalog.pins.push_back(
                    std::make_shared<ResourceVersionLease>(ResourceVersionLease{cpu, std::make_shared<const ResourceBacking>(backing)}));
            }
        }
        const std::array materials{PreparedMaterialTechniques{materialKey, {{Name("opaque"), std::move(layout)}}}};
        const auto       indexed = BuildPreparedRasterIndex(catalog, meshes, materials);
        ASSERT_TRUE(std::holds_alternative<PreparedRasterIndex>(indexed));
        EXPECT_TRUE(std::holds_alternative<FrameError>(Resolve(std::get<PreparedRasterIndex>(indexed))));
        catalog.pins.resize(originalCount);
    }
}

TEST_F(PreparedRasterResolution, UnavailableImagesDoNotHideInvalidVertexInputs)
{
    const auto attributes = meshes.front().attributes;
    for (const bool duplicate : {false, true}) {
        SCOPED_TRACE(duplicate);
        meshes.front().attributes = attributes;
        if (duplicate)
            meshes.front().attributes.push_back(attributes.front());
        else
            meshes.front().attributes.front().format = Format::F_RG32;
        const std::array materials{PreparedMaterialTechniques{materialKey, {{Name("opaque"), Layout(true, true, true)}}}};
        const auto       indexed = BuildPreparedRasterIndex(catalog, meshes, materials);
        ASSERT_TRUE(std::holds_alternative<PreparedRasterIndex>(indexed));
        const auto &index = std::get<PreparedRasterIndex>(indexed);
        EXPECT_FALSE(index.pins.contains(PreparedRasterIndex::Key(imageKey)));
        const auto resolved = Resolve(index);
        ASSERT_TRUE(std::holds_alternative<FrameError>(resolved));
        EXPECT_EQ(std::get<FrameError>(resolved).code, FrameErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(std::get<FrameError>(resolved).message,
                  duplicate ? "Mesh has duplicate shader input semantics" : "Mesh vertex format does not match the shader input type");
    }
}

TEST_F(PreparedRasterResolution, GraphLocalBuffersAndRepeatedViewsUseTheSameExactIndex)
{
    const std::array   materials{PreparedMaterialTechniques{materialKey, {{Name("opaque"), Layout(false, false)}}}};
    Buffer::Descriptor descriptor;
    descriptor.size  = 16;
    descriptor.usage = BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX;
    const auto local = builder.CreateTransient(descriptor);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(local));
    const auto       handle = std::get<GraphBufferHandle>(local);
    const std::array buffers{PreparedGraphBuffer{meshKey, handle}};
    const auto       indexed = BuildPreparedRasterIndex(catalog, meshes, materials, buffers);
    ASSERT_TRUE(std::holds_alternative<PreparedRasterIndex>(indexed));
    const auto &index = std::get<PreparedRasterIndex>(indexed);
    for (uint32_t i = 0; i < 2; ++i) {
        view.draws.front().world.v[12] = static_cast<float>(i);
        const auto resolved            = Resolve(index);
        ASSERT_TRUE(std::holds_alternative<PreparedRasterView>(resolved));
        const auto &draw = std::get<PreparedRasterView>(resolved).draws.front();
        EXPECT_EQ(draw.draw.vertices.front().range.buffer, handle);
        EXPECT_EQ(draw.world.v[12], static_cast<float>(i));
    }
    ++view.draws.front().material.resource.revision;
    EXPECT_FALSE(index.GeometryReady(view.draws.front()));
    EXPECT_TRUE(std::holds_alternative<FrameError>(Resolve(index)));
}
