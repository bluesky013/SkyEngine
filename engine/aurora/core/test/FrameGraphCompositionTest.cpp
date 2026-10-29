//
// Created on 2026/10/27.
//

#include "AuroraTestHelper.h"
#include "RendererRuntime.h"
#include "graph/FrameComposer.h"

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

TEST(RendererConfiguration, UnknownVulkanModesAreRejectedBeforeBackendAcquisition)
{
    auto *originalDevice = Instance::Get()->GetDevice();
    auto  provider       = CreateRendererProvider();
    for (const auto value : {3u, 255u}) {
        RendererInitInfo info;
        info.vulkanMode = static_cast<RendererVulkanMode>(value);
        auto provided   = provider->Create(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(provided));
        EXPECT_EQ(std::get<RendererError>(provided).code, RendererErrorCode::INVALID_ARGUMENT);
        auto direct = CreateRuntimeRenderer(info);
        ASSERT_TRUE(std::holds_alternative<RendererError>(direct));
        EXPECT_EQ(std::get<RendererError>(direct).code, RendererErrorCode::INVALID_ARGUMENT);
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
