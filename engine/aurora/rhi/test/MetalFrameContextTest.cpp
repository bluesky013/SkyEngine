#include "AuroraTestHelper.h"
#include "ShaderArtifactTestHelper.h"

#include <array>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrameBindings.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/SubmitInfo.h>
#include <bit>
#include <chrono>
#include <core/archive/BinaryData.h>
#include <cstring>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
namespace {
    constexpr uint32_t kSize    = 8;
    constexpr uint64_t kGraphId = 1;

    std::shared_ptr<const FrozenFrameGraph>
    BuildClearReadbackGraph(const ImagePtr &image, const Image::Descriptor &imageDesc, const BufferPtr &readback)
    {
        FrameGraphData data;

        FrameGraphBuffer buffer;
        buffer.buffer  = readback;
        buffer.size    = kSize * kSize * 4;
        buffer.initial = CopyBufferState::COMMON;
        buffer.final   = CopyBufferState::HOST_READ;
        data.buffers.push_back(buffer);

        FrameGraphImage target;
        target.image      = FrameImageReference(image);
        target.descriptor = imageDesc;
        target.initial    = ImageLayout::UNDEFINED;
        target.final      = ImageLayout::TRANSFER_SRC;
        data.images.push_back(target);

        FrameRasterPass clear;
        clear.color = GraphImageHandle{kGraphId, 0};
        clear.load  = LoadOp::CLEAR;
        clear.clear = ClearValue(0.25f, 0.5f, 0.75f, 1.0f);
        data.passes.push_back(clear);

        BufferImageCopyPass copy;
        copy.buffer                 = GraphBufferHandle{kGraphId, 0};
        copy.image                  = GraphImageHandle{kGraphId, 0};
        copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
        copy.region.imageExtent     = {kSize, kSize, 1};
        copy.region.subRange.layers = 1;
        data.passes.push_back(copy);

        data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, kSize * kSize * 4});
        return std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    }

    ShaderPtr MakeMetalFixtureShader(Device *device, const char *source, ShaderReflection reflection, bool graphics)
    {
        std::vector<ShaderBindingPolicy> policies;
        for (auto &resource : reflection.resources) {
            resource.declarationOwner = "rhi-test/metal-frame-context";
            if (resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                policies.push_back({resource.set, resource.binding, resource.type});
                resource.type = resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ? ShaderResourceType::UNIFORM_BUFFER
                                                                                            : ShaderResourceType::STORAGE_BUFFER;
            }
        }
        reflection.declarations   = reflection.resources;
        reflection.declaredBlocks = reflection.blocks;
        auto makeFunction         = [&](ShaderStageFlagBit stage, const char *entry) {
            auto target = reflection;
            std::erase_if(target.resources, [&](const auto &resource) { return !resource.stages.TestBit(stage); });
            for (auto &resource : target.resources)
                resource.stages = stage;
            std::erase_if(target.blocks, [&](const auto &block) {
                return std::none_of(target.resources.begin(), target.resources.end(),
                                            [&](const auto &resource) { return resource.set == block.set && resource.binding == block.binding; });
            });
            std::erase_if(target.nativeBindings, [&](const auto &binding) { return binding.stage != stage; });
            const auto *bytes    = reinterpret_cast<const uint8_t *>(source);
            auto        artifact = MakeTestShaderArtifact(ShaderTarget::MSL, stage, {bytes, std::strlen(source)}, entry, std::move(target));
            if (!artifact)
                return ShaderFunctionPtr{};
            ShaderFunction::Descriptor descriptor{};
            descriptor.stage    = stage;
            descriptor.artifact = std::move(artifact);
            return ShaderFunctionPtr(device->CreateShaderFunction(descriptor));
        };
        auto first    = makeFunction(graphics ? ShaderStageFlagBit::VS : ShaderStageFlagBit::CS, graphics ? "VSMain" : "CSMain");
        auto fragment = graphics ? makeFunction(ShaderStageFlagBit::FS, "FSMain") : ShaderFunctionPtr{};
        if (!first || (graphics && !fragment))
            return {};
        ShaderReflection program;
        std::string      error;
        if (!MergeShaderReflections(first->GetArtifact()->reflection, program, error) ||
            (fragment && !MergeShaderReflections(fragment->GetArtifact()->reflection, program, error)))
            return {};
        for (const auto &policy : policies) {
            const auto found = std::find_if(program.resources.begin(), program.resources.end(),
                                            [&](const auto &resource) { return resource.set == policy.set && resource.binding == policy.binding; });
            if (found == program.resources.end())
                return {};
            found->type = policy.type;
        }
        Shader::Descriptor descriptor{};
        if (graphics) {
            descriptor.vs = first.Get();
            descriptor.ps = fragment.Get();
        } else
            descriptor.cs = first.Get();
        descriptor.reflection      = &program;
        descriptor.bindingPolicies = std::move(policies);
        return ShaderPtr(device->CreateShader(descriptor));
    }

    ShaderBlockLayout FixtureUniformBlock(uint32_t set, uint32_t binding, const char *name, bool vector)
    {
        ShaderBlockLayout block;
        block.name       = name;
        block.structName = vector ? "float4" : "Params";
        block.set        = set;
        block.binding    = binding;
        block.size       = vector ? 16 : 4;
        block.alignment  = vector ? 16 : 4;
        ShaderBlockMember member;
        member.name       = vector ? "color" : "value";
        member.size       = block.size;
        member.alignment  = block.alignment;
        member.scalarType = vector ? ShaderScalarType::FLOAT : ShaderScalarType::UINT;
        member.kind       = vector ? ShaderTypeKind::VECTOR : ShaderTypeKind::SCALAR;
        member.cols       = vector ? 4 : 1;
        block.members.push_back(member);
        return block;
    }

    ShaderPtr MakeFullscreenShader(Device *device, const char *fragmentSource = nullptr, const ShaderReflection *reflectionOverride = nullptr)
    {
        const char *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 position [[position]]; };
vertex VOut VSMain(uint vid [[vertex_id]]) {
    const float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    VOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    return out;
}
fragment float4 FSMain(VOut in [[stage_in]]) {
    return float4(0.0, 1.0, 0.0, 1.0);
}
)MSL";
        if (fragmentSource != nullptr)
            source = fragmentSource;

        return MakeMetalFixtureShader(device, source, reflectionOverride ? *reflectionOverride : ShaderReflection{}, true);
    }

    bool WaitForCompletion(DeviceFrameContext &context)
    {
        for (uint32_t i = 0; i < 2000; ++i) {
            std::array<FrameCompletion, 1> completion{};
            if (context.PollCompleted(completion) != 0)
                return completion[0].status == FrameCompletionStatus::COMPLETE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }
} // namespace

TEST_F(AuroraMetalTest, SamplerRejectsUnavailableAddressModes)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    for (const auto mode : {WrapMode::CLAMP_TO_BORDER, WrapMode::MIRROR_CLAMP_TO_EDGE}) {
        bool supported = false;
        if (mode == WrapMode::CLAMP_TO_BORDER) {
            if (__builtin_available(macOS 10.12, iOS 14.0, *))
                supported = true;
        } else if (__builtin_available(macOS 10.11, iOS 14.0, *))
            supported = true;
        Sampler::Descriptor desc;
        desc.addressModeU = mode;
        SamplerPtr sampler(device->CreateSampler(desc));
        if (!supported)
            EXPECT_FALSE(sampler);
        if (sampler)
            EXPECT_EQ(sampler->GetDevice(), device);
    }
    Sampler::Descriptor invalid;
    invalid.addressModeU = static_cast<WrapMode>(UINT32_MAX);
    SamplerPtr sampler(device->CreateSampler(invalid));
    EXPECT_FALSE(sampler);
}

TEST_F(AuroraMetalTest, QueueRejectsUnknownCommandsWithoutSubmittingFence)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    struct UnknownCommand final : CommandBuffer {
        void Begin() override
        {
        }
        void End() override
        {
        }
        void PipelineBarrier(const BarrierInfo &) override
        {
        }
        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override
        {
            return {};
        }
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override
        {
            return {};
        }
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override
        {
            return {};
        }
    } unknown;
    auto fence = MakeFence(device, true);
    ASSERT_TRUE(fence);
    EXPECT_EQ(fence->GetDevice(), device);
    ASSERT_TRUE(fence->TryReset().Succeeded());
    SubmitInfo submission;
    submission.commandBuffers = {&unknown};
    submission.fence          = fence.Get();
    auto *queue               = device->GetQueue(QueueType::GRAPHICS);
    EXPECT_EQ(queue->TrySubmit(submission).status, QueueSubmitStatus::REJECTED);
    EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
    std::unique_ptr<CommandPool> pool(device->CreateCommandPool(QueueType::GRAPHICS));
    ASSERT_TRUE(pool);
    auto *command = pool->Allocate();
    ASSERT_TRUE(command);
    command->Begin();
    command->End();
    submission.commandBuffers = {command, command};
    EXPECT_EQ(queue->TrySubmit(submission).status, QueueSubmitStatus::REJECTED);
    EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
    submission.commandBuffers = {command};
    EXPECT_TRUE(fence->TryReset().Succeeded());
    ASSERT_EQ(queue->TrySubmit(submission).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_TRUE(fence->WaitFor(2'000'000'000));
    EXPECT_EQ(fence->Query().status, FenceStatus::SIGNALED);
    EXPECT_EQ(queue->TrySubmit(submission).status, QueueSubmitStatus::REJECTED);
}

TEST_F(AuroraMetalTest, RenderBarrierUsesAvailabilityAndRejectsBeforeSubmission)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    Image::Descriptor descriptor;
    descriptor.format = PixelFormat::RGBA8_UNORM;
    descriptor.extent = {kSize, kSize, 1};
    descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET;
    ImagePtr image(device->CreateImage(descriptor));
    ASSERT_TRUE(image);
    auto fence  = MakeFence(device);
    auto signal = MakeTimelineSema(device);
    ASSERT_TRUE(fence && signal);
    std::unique_ptr<CommandPool> pool(device->CreateCommandPool(QueueType::GRAPHICS));
    ASSERT_TRUE(pool);
    auto *command = pool->Allocate();
    ASSERT_TRUE(command);
    BarrierInfo barrier;
    barrier.srcStage = PipelineStageBit::FRAGMENT_SHADER;
    barrier.dstStage = PipelineStageBit::FRAGMENT_SHADER;
    barrier.memoryBarriers.push_back({AccessFlagBit::UAV, AccessFlagBit::SRV});
    SubmitInfo submission;
    submission.commandBuffers   = {command};
    submission.fence            = fence.Get();
    submission.signalSemaphores = {{signal.Get(), 7}};
    bool supported              = false;
    if (__builtin_available(macOS 10.14, iOS 16.0, *))
        supported = true;

    for (const bool deferred : {false, true}) {
        submission.signalSemaphores[0].value = deferred ? 8 : 7;
        ASSERT_TRUE(fence->TryReset().Succeeded());
        command->Begin();
        if (deferred)
            command->PipelineBarrier(barrier);
        {
            auto encoder = command->CreateGraphicsEncoder();
            ASSERT_TRUE(encoder);
            RenderingInfo info;
            info.renderArea        = {{0, 0}, {kSize, kSize}};
            info.numColors         = 1;
            info.colors[0].image   = image.Get();
            info.colors[0].loadOp  = LoadOp::CLEAR;
            info.colors[0].storeOp = StoreOp::STORE;
            encoder->BeginRendering(info);
            if (!deferred) {
                command->PipelineBarrier(barrier);
                command->PipelineBarrier(barrier);
            }
            encoder->EndRendering();
        }
        command->End();
        const auto result = device->GetQueue(QueueType::GRAPHICS)->TrySubmit(submission);
        if (supported) {
            ASSERT_EQ(result.status, QueueSubmitStatus::ACCEPTED);
            EXPECT_TRUE(fence->WaitFor(2'000'000'000));
            EXPECT_EQ(signal->GetCurrentValue(), submission.signalSemaphores[0].value);
        } else {
            EXPECT_EQ(result.status, QueueSubmitStatus::REJECTED);
            EXPECT_EQ(result.error.code, GpuErrorCode::UNSUPPORTED);
            EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
            EXPECT_EQ(signal->GetCurrentValue(), 0);
        }
    }
    command->Begin();
    command->End();
    submission.signalSemaphores[0].value = 9;
    ASSERT_TRUE(fence->TryReset().Succeeded());
    ASSERT_EQ(device->GetQueue(QueueType::GRAPHICS)->TrySubmit(submission).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_TRUE(fence->WaitFor(2'000'000'000));
    EXPECT_EQ(signal->GetCurrentValue(), 9);
}

TEST_F(AuroraMetalTest, QueueWaitPrecedesAlreadyRecordedCopy)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    Buffer::Descriptor desc;
    desc.size   = 16;
    desc.memory = MemoryType::CPU_TO_GPU;
    desc.usage  = BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST;
    BufferPtr source(device->CreateBuffer(desc)), target(device->CreateBuffer(desc));
    ASSERT_TRUE(source && target);
    auto *sourceBytes = source->Map();
    auto *targetBytes = target->Map();
    ASSERT_TRUE(sourceBytes && targetBytes);
    std::memset(sourceBytes, 0x5a, 16);
    std::memset(targetBytes, 0, 16);
    source->UnMap();
    target->UnMap();
    auto gate  = MakeTimelineSema(device);
    auto fence = MakeFence(device);
    ASSERT_TRUE(gate && fence);
    EXPECT_EQ(gate->GetDevice(), device);
    std::unique_ptr<CommandPool> pool(device->CreateCommandPool(QueueType::GRAPHICS));
    ASSERT_TRUE(pool);
    auto *command = pool->Allocate();
    ASSERT_TRUE(command);
    command->Begin();
    {
        auto copy = command->CreateBlitEncoder();
        copy->CopyBuffer(source.Get(), target.Get(), 16, 0, 0);
    }
    command->End();
    SubmitInfo submission;
    submission.commandBuffers = {command};
    submission.waitSemaphores = {{gate.Get(), 1, PipelineStageBit::TRANSFER}};
    submission.fence          = fence.Get();
    const auto submitted      = device->GetQueue(QueueType::GRAPHICS)->TrySubmit(submission);
    // Release the gate even after a failed check so test teardown cannot strand GPU work.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto              beforeSignal = fence->Query();
    std::array<uint8_t, 16> before{};
    std::memcpy(before.data(), targetBytes, 16);
    gate->Signal(1);
    ASSERT_EQ(submitted.status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(beforeSignal.status, FenceStatus::PENDING);
    EXPECT_EQ(before, (std::array<uint8_t, 16>{}));
    ASSERT_TRUE(fence->WaitFor(2'000'000'000));
    for (uint32_t i = 0; i < 16; ++i)
        EXPECT_EQ(targetBytes[i], 0x5a);
}

TEST_F(AuroraMetalTest, FenceCannotBeRearmedAcrossQueuesWhileSubmitted)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto gate  = MakeTimelineSema(device);
    auto fence = MakeFence(device);
    ASSERT_TRUE(gate && fence);
    SubmitInfo first;
    first.waitSemaphores = {{gate.Get(), 1, PipelineStageBit::TOP}};
    first.fence          = fence.Get();
    const auto submitted = device->GetQueue(QueueType::GRAPHICS)->TrySubmit(first);
    SubmitInfo second;
    second.fence      = fence.Get();
    const auto reused = device->GetQueue(QueueType::TRANSFER)->TrySubmit(second);
    const auto reset  = fence->TryReset();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto beforeSignal = fence->Query();
    gate->Signal(1);
    ASSERT_EQ(submitted.status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(reused.status, QueueSubmitStatus::REJECTED);
    EXPECT_FALSE(reset.Succeeded());
    EXPECT_EQ(beforeSignal.status, FenceStatus::PENDING);
    ASSERT_TRUE(fence->WaitFor(2'000'000'000));
    ASSERT_EQ(device->GetQueue(QueueType::TRANSFER)->TrySubmit(second).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_TRUE(fence->WaitFor(2'000'000'000));
}

TEST_F(AuroraMetalTest, FrameContextClearAndReadback)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imageDesc;
    imageDesc.format      = PixelFormat::RGBA8_UNORM;
    imageDesc.extent      = {kSize, kSize, 1};
    imageDesc.mipLevels   = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.samples     = SampleCount::X1;
    imageDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory      = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    auto graph   = BuildClearReadbackGraph(image, imageDesc, readback);
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);

    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto lease = std::get<DeviceFrameLease>(begun);

    auto compiled = context->Compile(lease, graph);
    if (auto *error = std::get_if<FrameError>(&compiled))
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));

    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto ticket = std::get<EncodingTicket>(dispatched);

    auto collected = context->TryCollect(ticket);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));

    auto submitted = context->Submit(recorded);
    if (auto *rejected = std::get_if<SubmissionRejected>(&submitted))
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        EXPECT_NE(pixels[i * 4 + 0], 0);
        EXPECT_NE(pixels[i * 4 + 1], 0);
        EXPECT_NE(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_F(AuroraMetalTest, RasterDynamicUniformOffsetsRemainDistinctAcrossDraws)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const uint32_t     stride = std::max(256u, device->GetCapability().minUniformBufferOffsetAlignment);
    Buffer::Descriptor uniformDesc;
    uniformDesc.size   = uint64_t(stride) * 4;
    uniformDesc.usage  = BufferUsageFlagBit::UNIFORM;
    uniformDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr uniform(device->CreateBuffer(uniformDesc));
    ASSERT_TRUE(uniform);
    auto *mapped = uniform->Map();
    ASSERT_TRUE(mapped);
    std::memset(mapped, 0, uniformDesc.size);
    const std::array<float, 4> values[] = {{.2f, 0, 0, 0}, {0, .8f, 0, 0}, {.6f, .3f, 0, 0}};
    for (uint32_t i = 0; i < 3; ++i)
        std::memcpy(mapped + uint64_t(i + 1) * stride, values[i].data(), sizeof(values[i]));
    uniform->UnMap();
    ShaderReflection reflection;
    for (const uint32_t binding : {3u, 1u}) {
        ShaderResource resource;
        resource.type    = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        resource.name    = binding == 3 ? "a" : "b";
        resource.set     = 1;
        resource.binding = binding;
        resource.stages  = binding == 3 ? ShaderStageFlagBit::VS | ShaderStageFlagBit::FS : ShaderStageFlags(ShaderStageFlagBit::FS);
        reflection.resources.push_back(resource);
        reflection.blocks.push_back(FixtureUniformBlock(1, binding, resource.name.c_str(), true));
    }
    reflection.nativeBindings = {{1, 3, ShaderStageFlagBit::VS, 2, ShaderNativeCategory::METAL_BUFFER},
                                 {1, 3, ShaderStageFlagBit::FS, 5, ShaderNativeCategory::METAL_BUFFER},
                                 {1, 1, ShaderStageFlagBit::FS, 9, ShaderNativeCategory::METAL_BUFFER}};
    auto shader               = MakeFullscreenShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 position [[position]]; float red; };
vertex VOut VSMain(uint id [[vertex_id]], constant float4 &a [[buffer(2)]]) {
    const float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    VOut result; result.position = float4(positions[id], 0, 1); result.red = a.r; return result;
}
fragment float4 FSMain(VOut input [[stage_in]], constant float4 &a [[buffer(5)]], constant float4 &b [[buffer(9)]]) {
    return float4(input.red, b.g, a.r, 1);
}
)MSL",
                                                     &reflection);
    ASSERT_TRUE(shader);
    Image::Descriptor imageDesc;
    imageDesc.format = PixelFormat::RGBA8_UNORM;
    imageDesc.extent = {kSize, kSize, 1};
    imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_TRUE(image);
    Buffer::Descriptor readbackDesc;
    readbackDesc.size   = kSize * kSize * 4;
    readbackDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    readbackDesc.memory = MemoryType::GPU_TO_CPU;
    BufferPtr readback(device->CreateBuffer(readbackDesc));
    ASSERT_TRUE(readback);
    auto             data = BuildClearReadbackGraph(image, imageDesc, readback)->GetData();
    FrameGraphBuffer constants;
    constants.buffer      = uniform;
    constants.size        = uniformDesc.size;
    constants.initial     = CopyBufferState::HOST_WRITE;
    constants.final       = CopyBufferState::SHADER_READ;
    constants.initialized = true;
    data.buffers.push_back(constants);
    FrameRasterPass pass;
    pass.color = {kGraphId, 0};
    pass.load  = LoadOp::LOAD;
    for (uint32_t i = 0; i < 2; ++i) {
        RasterDraw draw;
        draw.shader              = shader;
        draw.command.vertexCount = 3;
        draw.scissor             = {{static_cast<int32_t>(i * 4), 0}, {4, kSize}};
        RasterBindingDraft bindings;
        bindings.uniforms.push_back({1, 3, {{kGraphId, 1}, 0, 16}, (i ? 3u : 1u) * stride});
        bindings.uniforms.push_back({1, 1, {{kGraphId, 1}, 0, 16}, (i ? 3u : 2u) * stride});
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
        pass.draws.push_back(std::move(draw));
    }
    data.passes.insert(data.passes.begin() + 1, std::move(pass));
    auto                                graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));
    auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    const std::array<int, 3> expected[] = {{51, 204, 51}, {153, 77, 153}};
    for (uint32_t i = 0; i < 2; ++i)
        for (uint32_t channel = 0; channel < 3; ++channel)
            EXPECT_NEAR(pixels[(4 * kSize + 2 + i * 4) * 4 + channel], expected[i][channel], 2);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextDrawsFullscreenTriangle)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imageDesc;
    imageDesc.format      = PixelFormat::RGBA8_UNORM;
    imageDesc.extent      = {kSize, kSize, 1};
    imageDesc.mipLevels   = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.samples     = SampleCount::X1;
    imageDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory      = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage target;
    target.image      = FrameImageReference(image);
    target.descriptor = imageDesc;
    target.initial    = ImageLayout::UNDEFINED;
    target.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(target);

    FrameRasterPass raster;
    raster.color = GraphImageHandle{kGraphId, 0};
    raster.load  = LoadOp::CLEAR;
    raster.clear = ClearValue(0.25f, 0.5f, 0.75f, 1.0f);
    raster.draws.push_back({MakeFullscreenShader(device), {}, {3, 1, 0, 0}, {}});
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                 = GraphBufferHandle{kGraphId, 0};
    copy.image                  = GraphImageHandle{kGraphId, 0};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = {kSize, kSize, 1};
    copy.region.subRange.layers = 1;
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));

    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    if (auto *error = std::get_if<FrameError>(&compiled))
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));

    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded  = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    auto submitted = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        EXPECT_EQ(pixels[i * 4 + 0], 0);
        EXPECT_EQ(pixels[i * 4 + 1], 255);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

namespace {
    ShaderFunctionPtr MetalConstantFunction(Device *device, const char *source, ShaderStageFlagBit stage,
                                            std::vector<ShaderVariantBinding> bindings = {})
    {
        ShaderFunction::Descriptor desc{};
        desc.stage        = stage;
        const auto *bytes = reinterpret_cast<const uint8_t *>(source);
        const auto base = MakeTestShaderArtifact(ShaderTarget::MSL, stage, {bytes, std::strlen(source)}, stage == ShaderStageFlagBit::VS ? "VSMain" : "FSMain");
        if (!base)
            return {};
        auto artifact = *base;
        if (bindings.empty()) {
            if (stage == ShaderStageFlagBit::VS)
                bindings = {{"FLIP", "flip", ShaderVariantType::BOOL, ShaderVariantLowering::NATIVE, 1, 0}};
            else
                bindings = {{"SIGNED", "signedValue", ShaderVariantType::INT32, ShaderVariantLowering::NATIVE, 0, 1},
                            {"ENABLED", "enabled", ShaderVariantType::BOOL, ShaderVariantLowering::NATIVE, 1, 0},
                            {"UNSIGNED", "unsignedValue", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 2, 2},
                            {"MULTIPLIER", "multiplier", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 3, 5}};
        }
        artifact.variantBindings = std::move(bindings);
        std::string error;
        if (!SealShaderArtifact(artifact, error))
            return {};
        desc.artifact = std::make_shared<const ShaderStageArtifact>(std::move(artifact));
        return ShaderFunctionPtr(device->CreateShaderFunction(desc));
    }
    ShaderPtr MetalConstantProgram(Device                     *device,
                                   const ShaderFunctionPtr    &vertex,
                                   const ShaderFunctionPtr    &fragment,
                                   const ShaderSpecialization *values = nullptr)
    {
        if (!vertex || !fragment)
            return {};
        ShaderReflection reflection;
        std::string      error;
        if (!MergeShaderReflections(vertex->GetArtifact()->reflection, reflection, error) ||
            !MergeShaderReflections(fragment->GetArtifact()->reflection, reflection, error))
            return {};
        Shader::Descriptor desc{};
        desc.vs             = vertex.Get();
        desc.ps             = fragment.Get();
        desc.reflection     = &reflection;
        desc.specialization = values;
        return ShaderPtr(device->CreateShader(desc));
    }
    constexpr const char *MetalConstantVertex   = R"MSL(
        #include <metal_stdlib>
        using namespace metal;
        constant bool flip [[function_constant(1)]];
        struct Vertex { float4 position [[position]]; };
        vertex Vertex VSMain(uint id [[vertex_id]]) {
            const float2 positions[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
            Vertex v;
            v.position = float4(positions[id], 0, 1);
            if (is_function_constant_defined(flip) && flip)
                v.position.y = -v.position.y;
            return v;
        }
    )MSL";
    constexpr const char *MetalConstantFragment = R"MSL(
        #include <metal_stdlib>
        using namespace metal;
        constant int signedValue [[function_constant(0)]];
        constant bool enabled [[function_constant(1)]];
        constant uint unsignedValue [[function_constant(2)]];
        constant uint multiplier [[function_constant(3)]];
        fragment float4 FSMain() {
            const int i = is_function_constant_defined(signedValue) ? signedValue : 1;
            const bool b = is_function_constant_defined(enabled) ? enabled : false;
            const uint u = is_function_constant_defined(unsignedValue) ? unsignedValue : 2;
            const float f = is_function_constant_defined(multiplier) ? float(multiplier) * 0.1f : 0.5f;
            return float4(float(i) * 0.1f, b ? 0.8f : 0.2f, float(u) * f * 0.1f, 1);
        }
    )MSL";
} // namespace

TEST_F(AuroraMetalTest, SpecializationValuesProducePrivateFunctionsAndPreserveSharedDefaults)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const auto vertex   = MetalConstantFunction(device, MetalConstantVertex, ShaderStageFlagBit::VS);
    const auto fragment = MetalConstantFunction(device, MetalConstantFragment, ShaderStageFlagBit::FS);
    ASSERT_TRUE(vertex && fragment);
    ShaderSpecialization first, second;
    first.entries = {{0, 3, ShaderStageFlagBit::FS, ShaderVariantType::INT32, "SIGNED"},
                     {1, 1, ShaderStageFlagBit::FS, ShaderVariantType::BOOL, "ENABLED"},
                     {2, 4, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "UNSIGNED"},
                     {3, 5, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "MULTIPLIER"}};
    second.entries = {{0, 7, ShaderStageFlagBit::FS, ShaderVariantType::INT32, "SIGNED"},
                      {1, 0, ShaderStageFlagBit::FS, ShaderVariantType::BOOL, "ENABLED"},
                      {2, 2, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "UNSIGNED"},
                      {3, 10, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "MULTIPLIER"}};
    const std::array shaders{MetalConstantProgram(device, vertex, fragment), MetalConstantProgram(device, vertex, fragment, &first),
                             MetalConstantProgram(device, vertex, fragment, &second), MetalConstantProgram(device, vertex, fragment)};
    for (const auto &shader : shaders)
        ASSERT_TRUE(shader);
    Image::Descriptor imageDesc;
    imageDesc.format = PixelFormat::RGBA8_UNORM;
    imageDesc.extent = {kSize * 4, kSize, 1};
    imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_TRUE(image);
    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = uint64_t(imageDesc.extent.width) * imageDesc.extent.height * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(readback);
    FrameGraphData data;
    data.buffers.push_back({readback, bufferDesc.size, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
    FrameGraphImage target;
    target.image      = FrameImageReference(image);
    target.descriptor = imageDesc;
    target.initial    = ImageLayout::UNDEFINED;
    target.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(target);
    FrameRasterPass pass;
    pass.color = {kGraphId, 0};
    for (uint32_t index = 0; index < shaders.size(); ++index) {
        RasterDraw draw;
        draw.shader              = shaders[index];
        draw.command.vertexCount = 3;
        draw.scissor             = {{static_cast<int32_t>(index * kSize), 0}, {kSize, kSize}};
        pass.draws.push_back(std::move(draw));
    }
    data.passes.emplace_back(std::move(pass));
    BufferImageCopyPass copy;
    copy.buffer                 = {kGraphId, 0};
    copy.image                  = {kGraphId, 0};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = imageDesc.extent;
    copy.region.subRange.layers = 1;
    data.passes.emplace_back(copy);
    data.exports.push_back({{kGraphId, 0}, 0, bufferDesc.size});
    const auto                          graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));
    const auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    const std::array colors{std::array{25.5f, 51.f, 25.5f}, std::array{76.5f, 204.f, 51.f}, std::array{178.5f, 51.f, 51.f},
                            std::array{25.5f, 51.f, 25.5f}};
    for (uint32_t index = 0; index < colors.size(); ++index) {
        const auto offset = (kSize / 2 * imageDesc.extent.width + index * kSize + kSize / 2) * 4;
        for (uint32_t channel = 0; channel < 3; ++channel)
            EXPECT_NEAR(pixels[offset + channel], colors[index][channel], 1) << "shader " << index << " channel " << channel;
        EXPECT_EQ(pixels[offset + 3], 255u);
    }
    readback->UnMap();
}

TEST_F(AuroraMetalTest, SpecializationRejectsInvalidValuesAndAllowsIndependentStageTypes)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const auto vertex   = MetalConstantFunction(device, MetalConstantVertex, ShaderStageFlagBit::VS);
    const auto fragment = MetalConstantFunction(device, MetalConstantFragment, ShaderStageFlagBit::FS);
    ASSERT_TRUE(vertex && fragment);
    for (const auto &entries : {std::vector<ShaderSpecializationEntry>{{99, 1}}, std::vector<ShaderSpecializationEntry>{{0, 1, ShaderStageFlagBit::FS, ShaderVariantType::INT32, "SIGNED"}, {0, 2, ShaderStageFlagBit::FS, ShaderVariantType::INT32, "SIGNED"}},
                                std::vector<ShaderSpecializationEntry>{{1, 2, ShaderStageFlagBit::FS, ShaderVariantType::BOOL, "ENABLED"}}}) {
        const ShaderSpecialization values{entries};
        EXPECT_FALSE(MetalConstantProgram(device, vertex, fragment, &values));
    }
    const auto conflicting = MetalConstantFunction(device, R"MSL(
        #include <metal_stdlib>
        using namespace metal;
        constant uint enabled [[function_constant(1)]];
        fragment float4 FSMain() {
            const uint value = is_function_constant_defined(enabled) ? enabled : 1;
            return float4(float(value), 0, 0, 1);
        }
    )MSL",
                                                   ShaderStageFlagBit::FS, {{"ENABLED", "enabled", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 1, 1}});
    ASSERT_TRUE(conflicting);
    const ShaderSpecialization conflict{{{1, 1, ShaderStageFlagBit::VS, ShaderVariantType::BOOL, "FLIP"},
                                         {1, 2, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "ENABLED"}}};
    EXPECT_TRUE(MetalConstantProgram(device, vertex, conflicting, &conflict));
    const auto unsupported = MetalConstantFunction(device, R"MSL(
        #include <metal_stdlib>
        using namespace metal;
        constant half halfValue [[function_constant(8)]];
        fragment float4 FSMain() {
            const half value = is_function_constant_defined(halfValue) ? halfValue : half(1);
            return float4(float(value), 0, 0, 1);
        }
    )MSL",
                                                   ShaderStageFlagBit::FS,
                                                   {{"HALF", "halfValue", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 8, 1}});
    ASSERT_TRUE(unsupported);
    const ShaderSpecialization halfValue{{{8, 1, ShaderStageFlagBit::FS, ShaderVariantType::UINT32, "HALF"}}};
    EXPECT_FALSE(MetalConstantProgram(device, vertex, unsupported, &halfValue));
}

TEST_F(AuroraMetalTest, FrameContextComputeUsesDynamicOffsetsInBindingOrder)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char      *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { uint value; };
kernel void CSMain(constant Params &first [[buffer(0)]], constant Params &second [[buffer(1)]],
                   device uint *output [[buffer(2)]], uint id [[thread_position_in_grid]]) {
    output[id] = first.value + 10 * second.value + id;
}
)MSL";
    ShaderReflection reflection;
    for (const auto binding : {1u, 3u}) {
        ShaderResource uniform;
        uniform.type    = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        uniform.name    = binding == 1 ? "first" : "second";
        uniform.binding = binding;
        uniform.stages  = ShaderStageFlagBit::CS;
        reflection.resources.push_back(uniform);
        reflection.blocks.push_back(FixtureUniformBlock(0, binding, uniform.name.c_str(), false));
        reflection.nativeBindings.push_back({0, binding, ShaderStageFlagBit::CS, binding == 1 ? 0u : 1u, ShaderNativeCategory::METAL_BUFFER});
    }
    ShaderResource storage;
    storage.name          = "output";
    storage.type          = ShaderResourceType::STORAGE_BUFFER;
    storage.set           = 1;
    storage.access        = ShaderResourceAccess::READ_WRITE;
    storage.bufferKind    = ShaderBufferKind::STRUCTURED;
    storage.elementStride = 4;
    storage.stages        = ShaderStageFlagBit::CS;
    reflection.resources.push_back(storage);
    reflection.nativeBindings.push_back({1, 0, ShaderStageFlagBit::CS, 2, ShaderNativeCategory::METAL_BUFFER});
    reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
    auto shader = MakeMetalFixtureShader(device, source, reflection, false);
    ASSERT_TRUE(shader);
    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = 1024;
    bufferDesc.usage  = BufferUsageFlagBit::UNIFORM;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    auto input        = BufferPtr(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(input);
    std::array<uint32_t, 256> values{};
    values[64]   = 11;
    values[128]  = 7;
    auto *mapped = input->Map();
    ASSERT_TRUE(mapped);
    std::memcpy(mapped, values.data(), sizeof(values));
    input->UnMap();
    bufferDesc.size   = 16;
    bufferDesc.usage  = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC;
    bufferDesc.memory = MemoryType::GPU_ONLY;
    auto output       = BufferPtr(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(output);
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    auto readback     = BufferPtr(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(readback);
    FrameGraphData data;
    data.buffers.push_back({input, 1024, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true});
    data.buffers.push_back({output, 16, CopyBufferState::COMMON, CopyBufferState::COMMON, false});
    data.buffers.push_back({readback, 16, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
    FrameComputePass compute;
    compute.shader  = std::move(shader);
    compute.groupsX = 4;
    ComputeBindingDraft bindings;
    bindings.buffers.push_back({0, 3, {{kGraphId, 0}, 0, 16}, 512, FrameAccessMode::READ});
    bindings.buffers.push_back({0, 1, {{kGraphId, 0}, 0, 16}, 256, FrameAccessMode::READ});
    bindings.buffers.push_back({1, 0, {{kGraphId, 1}, 0, 16}, 0, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(compute, std::move(bindings))));
    data.passes.emplace_back(std::move(compute));
    data.passes.emplace_back(BufferCopyPass{{kGraphId, 1}, {kGraphId, 2}, 0, 0, 16});
    data.exports.push_back({{kGraphId, 2}, 0, 16});
    const auto                          graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));
    const auto *result = reinterpret_cast<const uint32_t *>(readback->Map());
    ASSERT_TRUE(result);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(result[i], 81u + i);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextRejectsInvalidComputeDeclarations)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    FrameGraphData data;
    data.passes.push_back(FrameComputePass{});
    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));

    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
    EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::COMPILE_FAILED);
}

TEST_F(AuroraMetalTest, CustomFailureCanBeDiscardedBeforeSlotReuse)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        FrameOperationResult Encode(FrameCustomPassContext &context) const noexcept override
        {
            ++calls;
            auto encoder = context.command.CreateComputeEncoder();
            return FrameError{FrameErrorCode::ENCODE_FAILED, "Custom failed after creating an encoder"};
        }
    };
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    Buffer::Descriptor bufferDesc;
    bufferDesc.size  = kSize * kSize * 4;
    bufferDesc.usage = BufferUsageFlagBit::STORAGE;
    auto buffer      = BufferPtr(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(buffer);
    auto           first  = std::make_shared<Callback>();
    auto           second = std::make_shared<Callback>();
    FrameGraphData data;
    data.buffers.push_back({buffer, bufferDesc.size, CopyBufferState::COMMON, CopyBufferState::COMMON, false});
    for (const auto &callback : {first, second}) {
        FrameCustomPass pass;
        pass.encoder = callback;
        pass.buffers.push_back({0, 0, {{kGraphId, 0}, 0, bufferDesc.size}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
    }
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto lease    = std::get<DeviceFrameLease>(begun);
    auto       compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_FALSE(plan);
    const auto result = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(result));
    EXPECT_EQ(std::get<EncodingFailed>(result).error.code, FrameErrorCode::ENCODE_FAILED);
    EXPECT_EQ(first->calls, 1u);
    EXPECT_EQ(second->calls, 0u);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    Image::Descriptor imageDesc;
    imageDesc.format = PixelFormat::RGBA8_UNORM;
    imageDesc.extent = {kSize, kSize, 1};
    imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    const auto image = ImagePtr(device->CreateImage(imageDesc));
    ASSERT_TRUE(image);
    bufferDesc.usage    = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory   = MemoryType::CPU_TO_GPU;
    const auto readback = BufferPtr(device->CreateBuffer(bufferDesc));
    ASSERT_TRUE(readback);
    begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    compiled = context->Compile(std::get<DeviceFrameLease>(begun), BuildClearReadbackGraph(image, imageDesc, readback));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    plan.reset();
    ASSERT_TRUE(WaitForCompletion(*context));
    const auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    EXPECT_NEAR(pixels[0], 64, 1);
    EXPECT_NEAR(pixels[1], 128, 1);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextRejectsForeignAndForgedPlansBeforeConsumingIssuedPlan)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            ++calls;
            return FrameOperationComplete{};
        }
    };
    class UnknownPlan final : public BackendCompiledFrame {
    public:
        CompiledFrameIdentity identity;
        CompiledFrameIdentity GetIdentity() const noexcept override
        {
            return identity;
        }
    };
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    std::unique_ptr<DeviceFrameContext> first(device->CreateFrameContext({1, 1, 1}));
    std::unique_ptr<DeviceFrameContext> second(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(first && second);
    const auto      callback = std::make_shared<Callback>();
    FrameGraphData  data;
    FrameCustomPass pass;
    pass.encoder    = callback;
    pass.sideEffect = true;
    data.passes.emplace_back(std::move(pass));
    const auto begun = first->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto lease    = std::get<DeviceFrameLease>(begun);
    auto       compiled = first->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan                                      = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto forged                                    = std::make_unique<UnknownPlan>();
    forged->identity                               = plan->GetIdentity();
    std::unique_ptr<BackendCompiledFrame> unknown  = std::move(forged);
    const auto                            rejected = first->Dispatch(unknown);
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::WRONG_CONTEXT);
    EXPECT_TRUE(unknown);
    EXPECT_TRUE(std::holds_alternative<FrameError>(second->Dispatch(plan)));
    EXPECT_TRUE(plan);
    EXPECT_EQ(callback->calls, 0u);
    const auto dispatched = first->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_FALSE(plan);
    EXPECT_TRUE(std::holds_alternative<FrameError>(first->Dispatch(plan)));
    auto collected = first->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    EXPECT_EQ(callback->calls, 1u);
    std::get<std::unique_ptr<RecordedFrame>>(collected).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(first->DiscardUnsubmitted(lease)));
}

TEST_F(AuroraMetalTest, NativeRecordingFailureCannotPublishReceiptAndReleasesConsumedPlan)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        FrameOperationResult Encode(FrameCustomPassContext &context) const noexcept override
        {
            ++calls;
            Image         unknownOwner;
            RenderingInfo info;
            info.numColors       = 1;
            info.colors[0].image = &unknownOwner;
            auto encoder         = context.command.CreateGraphicsEncoder();
            encoder->BeginRendering(info);
            return FrameOperationComplete{};
        }
    };
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    const auto     callback = std::make_shared<Callback>();
    FrameGraphData data;
    for (uint32_t i = 0; i < 2; ++i) {
        FrameCustomPass pass;
        pass.encoder    = callback;
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
    }
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto lease    = std::get<DeviceFrameLease>(begun);
    auto       compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_FALSE(plan);
    const auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.code, FrameErrorCode::ENCODE_FAILED);
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.cause.code, GpuErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(callback->calls, 1u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(context->TryBeginSlot()));
}

TEST_F(AuroraMetalTest, ComputeInputValidationPreservesLease)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char                         *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void CSMain(constant uint &input [[buffer(0)]], device uint *output [[buffer(1)]], uint id [[thread_position_in_grid]]) {
    output[id] = input + id;
}
)MSL";
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    for (uint32_t fault = 0; fault < 10; ++fault) {
        SCOPED_TRACE(fault);
        ShaderReflection reflection;
        ShaderResource   uniform;
        uniform.name   = "input";
        uniform.type   = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        uniform.stages = ShaderStageFlagBit::CS;
        reflection.resources.push_back(uniform);
        reflection.blocks.push_back(FixtureUniformBlock(0, 0, "input", false));
        reflection.nativeBindings.push_back({0, 0, ShaderStageFlagBit::CS, 0, ShaderNativeCategory::METAL_BUFFER});
        ShaderResource storage;
        storage.name          = "output";
        storage.type          = ShaderResourceType::STORAGE_BUFFER;
        storage.binding       = 1;
        storage.access        = ShaderResourceAccess::READ_WRITE;
        storage.bufferKind    = ShaderBufferKind::STRUCTURED;
        storage.elementStride = 4;
        storage.stages        = ShaderStageFlagBit::CS;
        reflection.resources.push_back(storage);
        reflection.nativeBindings.push_back({0, 1, ShaderStageFlagBit::CS, 1, ShaderNativeCategory::METAL_BUFFER});
        reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        if (fault == 6)
            reflection.threadGroupSize[0] = 0;
        if (fault == 7)
            reflection.threadGroupSize[0] = UINT32_MAX;
        if (fault == 9)
            reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = 1024;
        ShaderPtr shader;
        if (fault == 0) {
            reflection.resources[0].stages     = ShaderStageFlagBit::VS;
            reflection.resources[1].stages     = ShaderStageFlagBit::FS;
            reflection.nativeBindings[0].stage = ShaderStageFlagBit::VS;
            reflection.nativeBindings[1].stage = ShaderStageFlagBit::FS;
            shader                             = MakeMetalFixtureShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 VSMain(uint id [[vertex_id]], constant uint &input [[buffer(0)]]) {
    return float4(float(id) + float(input), 0, 0, 1);
}
fragment float4 FSMain(device uint *output [[buffer(1)]]) {
    output[0] = 1; return float4(1);
}
)MSL",
                                                                        reflection, true);
        } else {
            shader = MakeMetalFixtureShader(device, source, reflection, false);
        }
        ASSERT_TRUE(shader);
        Buffer::Descriptor desc;
        desc.size   = 512;
        desc.usage  = fault == 1 ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::UNIFORM;
        desc.memory = fault == 8 ? static_cast<MemoryType>(255) : MemoryType::CPU_TO_GPU;
        auto input  = BufferPtr(device->CreateBuffer(desc));
        ASSERT_TRUE(input);
        desc.usage  = fault == 4 ? BufferUsageFlagBit::TRANSFER_DST : BufferUsageFlagBit::STORAGE;
        desc.memory = MemoryType::GPU_ONLY;
        auto output = BufferPtr(device->CreateBuffer(desc));
        ASSERT_TRUE(output);
        FrameGraphData data;
        data.buffers.push_back({input, 512, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true});
        data.buffers.push_back({output, 512, CopyBufferState::COMMON, CopyBufferState::COMMON, false});
        FrameComputePass pass;
        pass.shader  = std::move(shader);
        pass.groupsX = 1;
        ComputeBindingDraft bindings;
        bindings.buffers.push_back(
            {0, 0, {{kGraphId, 0}, fault == 2 ? 4u : 0u, 16}, fault == 3 ? 4u : (fault == 5 ? 512u : 0u), FrameAccessMode::READ});
        bindings.buffers.push_back({0, 1, {{kGraphId, 1}, 0, 16}, 0, FrameAccessMode::WRITE});
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(pass, std::move(bindings))));
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
        const auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        const auto lease    = std::get<DeviceFrameLease>(begun);
        const auto compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
        EXPECT_EQ(std::get<FrameError>(compiled).code, fault == 6 ? FrameErrorCode::UNSUPPORTED : FrameErrorCode::INVALID_ARGUMENT)
            << std::get<FrameError>(compiled).message;
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_F(AuroraMetalTest, StorageDynamicOffsetsUseNativeTypeAlignment)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char      *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void CSMain(device const uint *input [[buffer(0)]], device uint *output [[buffer(1)]]) {
    output[0] = input[0];
}
)MSL";
    ShaderReflection reflection;
    ShaderResource   inputResource;
    inputResource.name          = "input";
    inputResource.type          = ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
    inputResource.access        = ShaderResourceAccess::READ_ONLY;
    inputResource.bufferKind    = ShaderBufferKind::STRUCTURED;
    inputResource.elementStride = 4;
    inputResource.stages        = ShaderStageFlagBit::CS;
    ShaderResource outputResource;
    outputResource.name           = "output";
    outputResource.access         = ShaderResourceAccess::READ_WRITE;
    outputResource.bufferKind     = ShaderBufferKind::STRUCTURED;
    outputResource.elementStride  = 4;
    outputResource.stages         = ShaderStageFlagBit::CS;
    outputResource.type           = ShaderResourceType::STORAGE_BUFFER;
    outputResource.binding        = 1;
    reflection.resources          = {inputResource, outputResource};
    reflection.nativeBindings     = {{0, 0, ShaderStageFlagBit::CS, 0, ShaderNativeCategory::METAL_BUFFER},
                                     {0, 1, ShaderStageFlagBit::CS, 1, ShaderNativeCategory::METAL_BUFFER}};
    reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
    const auto shader = MakeMetalFixtureShader(device, source, reflection, false);
    ASSERT_TRUE(shader);
    Buffer::Descriptor desc;
    desc.size        = 16;
    desc.usage       = BufferUsageFlagBit::STORAGE;
    desc.memory      = MemoryType::CPU_TO_GPU;
    const auto input = BufferPtr(device->CreateBuffer(desc));
    ASSERT_TRUE(input);
    const std::array<uint32_t, 4> values{10, 20, 30, 40};
    auto                         *mapped = input->Map();
    ASSERT_TRUE(mapped);
    std::memcpy(mapped, values.data(), sizeof(values));
    input->UnMap();
    desc.memory       = MemoryType::GPU_TO_CPU;
    const auto output = BufferPtr(device->CreateBuffer(desc));
    ASSERT_TRUE(output);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    for (uint32_t offset : {1u, 4u}) {
        FrameGraphData data;
        data.buffers.push_back({input, 16, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true});
        data.buffers.push_back({output, 16, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
        FrameComputePass pass;
        pass.shader  = shader;
        ComputeBindingDraft bindings;
        bindings.buffers = {{0, 0, {{kGraphId, 0}, 4, 4}, offset, FrameAccessMode::READ}, {0, 1, {{kGraphId, 1}, 0, 4}, 0, FrameAccessMode::WRITE}};
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(pass, std::move(bindings))));
        data.passes.emplace_back(std::move(pass));
        data.exports.push_back({{kGraphId, 1}, 0, 4});
        auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        const auto lease    = std::get<DeviceFrameLease>(begun);
        auto       compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
        if (offset == 1) {
            ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
            EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::COMPILE_FAILED);
            EXPECT_EQ(std::get<FrameError>(compiled).message, "Compute storage buffer range does not match the shader storage shape");
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
            continue;
        }
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        plan.reset();
        ASSERT_TRUE(WaitForCompletion(*context));
        const auto *result = output->Map();
        ASSERT_TRUE(result);
        uint32_t value = 0;
        std::memcpy(&value, result, sizeof(value));
        EXPECT_EQ(value, 30u);
        output->UnMap();
    }
}

TEST_F(AuroraMetalTest, CustomWithoutResourcesExecutesAndRejectsForgedImageUsage)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            ++calls;
            return FrameOperationComplete{};
        }
    };
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    auto            callback = std::make_shared<Callback>();
    FrameCustomPass pass;
    pass.encoder    = callback;
    pass.sideEffect = true;
    FrameGraphData data;
    data.passes.emplace_back(pass);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), std::make_shared<const FrozenFrameGraph>(kGraphId, data));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    plan.reset();
    ASSERT_TRUE(WaitForCompletion(*context));
    EXPECT_EQ(callback->calls, 1u);

    FrameGraphImage image;
    image.descriptor.extent = {4, 4, 1};
    image.descriptor.format = PixelFormat::RGBA8_UNORM;
    image.descriptor.usage  = ImageUsageFlagBit::SAMPLED;
    image.image             = ImagePtr(device->CreateImage(image.descriptor));
    ASSERT_TRUE(image.image);
    image.descriptor.usage = ImageUsageFlagBit::STORAGE;
    image.initial = image.final = ImageLayout::GENERAL;
    image.initialized           = true;
    data.images.push_back(image);
    pass.images.push_back({0, 0, {kGraphId, 0}, {}, FrameAccessMode::READ, FrameImageBindingKind::STORAGE});
    data.passes[0] = pass;
    begun          = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    compiled = context->Compile(std::get<DeviceFrameLease>(begun), std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
    EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(callback->calls, 1u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(std::get<DeviceFrameLease>(begun))));
}

TEST_F(AuroraMetalTest, FrameContextResolvesMsaa)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor msaaDesc;
    msaaDesc.format  = PixelFormat::RGBA8_UNORM;
    msaaDesc.extent  = {kSize, kSize, 1};
    msaaDesc.samples = SampleCount::X4;
    msaaDesc.usage   = ImageUsageFlagBit::RENDER_TARGET;
    msaaDesc.memory  = MemoryType::GPU_ONLY;
    ImagePtr msaa(device->CreateImage(msaaDesc));
    ASSERT_NE(msaa, nullptr);

    Image::Descriptor resolveDesc;
    resolveDesc.format  = PixelFormat::RGBA8_UNORM;
    resolveDesc.extent  = {kSize, kSize, 1};
    resolveDesc.samples = SampleCount::X1;
    resolveDesc.usage   = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    resolveDesc.memory  = MemoryType::GPU_ONLY;
    ImagePtr resolve(device->CreateImage(resolveDesc));
    ASSERT_NE(resolve, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage msaaImage;
    msaaImage.image      = FrameImageReference(msaa);
    msaaImage.descriptor = msaaDesc;
    msaaImage.initial    = ImageLayout::UNDEFINED;
    msaaImage.final      = ImageLayout::COLOR_ATTACHMENT;
    data.images.push_back(msaaImage);

    FrameGraphImage resolveImage;
    resolveImage.image      = FrameImageReference(resolve);
    resolveImage.descriptor = resolveDesc;
    resolveImage.initial    = ImageLayout::UNDEFINED;
    resolveImage.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(resolveImage);

    FrameRasterPass raster;
    raster.color        = GraphImageHandle{kGraphId, 0};
    raster.colorResolve = RasterColorResolve{GraphImageHandle{kGraphId, 1}, {}};
    raster.load         = LoadOp::CLEAR;
    raster.clear        = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                 = GraphBufferHandle{kGraphId, 0};
    copy.image                  = GraphImageHandle{kGraphId, 1};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = {kSize, kSize, 1};
    copy.region.subRange.layers = 1;
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));

    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));

    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded  = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    auto submitted = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        EXPECT_EQ(pixels[i * 4 + 0], 51);
        EXPECT_EQ(pixels[i * 4 + 1], 102);
        EXPECT_EQ(pixels[i * 4 + 2], 204);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_F(AuroraMetalTest, ResolveHonorsColorAndDepthStoreActions)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    ShaderReflection reflection;
    ShaderResource   depthResource;
    depthResource.name    = "depth";
    depthResource.type    = ShaderResourceType::SAMPLED_IMAGE;
    depthResource.stages  = ShaderStageFlagBit::FS;
    depthResource.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 1, false, false};
    reflection.resources.push_back(depthResource);
    ShaderResource samplerResource;
    samplerResource.name    = "sam";
    samplerResource.type    = ShaderResourceType::SAMPLER;
    samplerResource.binding = 1;
    samplerResource.sampler = ShaderSamplerType::REGULAR;
    samplerResource.stages  = ShaderStageFlagBit::FS;
    reflection.resources.push_back(samplerResource);
    reflection.nativeBindings = {{0, 0, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_TEXTURE},
                                 {0, 1, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_SAMPLER}};
    const auto shader         = MakeFullscreenShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 VSMain(uint id [[vertex_id]]) {
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * 2 - 1, 0, 1);
}
fragment float4 FSMain(float4 p [[position]], depth2d<float> depth [[texture(0)]], sampler sam [[sampler(0)]]) {
    float d = depth.sample(sam, p.xy / 8, level(0));
    return float4(d, d, d, 1);
}
)MSL",
                                                     &reflection);
    ASSERT_TRUE(shader);
    Sampler::Descriptor samplerDesc;
    samplerDesc.minFilter = samplerDesc.magFilter = Filter::NEAREST;
    const SamplerPtr sampler(device->CreateSampler(samplerDesc));
    ASSERT_TRUE(sampler);
    constexpr uint64_t imageBytes = kSize * kSize * 4;
    for (const auto store : {StoreOp::DONT_CARE, StoreOp::STORE}) {
        SCOPED_TRACE(static_cast<uint32_t>(store));
        Image::Descriptor colorDesc;
        colorDesc.format         = PixelFormat::RGBA8_UNORM;
        colorDesc.extent         = {kSize, kSize, 1};
        colorDesc.samples        = SampleCount::X4;
        colorDesc.usage          = ImageUsageFlagBit::RENDER_TARGET;
        auto depthDesc           = colorDesc;
        depthDesc.format         = PixelFormat::D32;
        depthDesc.usage          = ImageUsageFlagBit::DEPTH_STENCIL;
        auto colorResolveDesc    = colorDesc;
        colorResolveDesc.samples = SampleCount::X1;
        colorResolveDesc.usage |= ImageUsageFlagBit::TRANSFER_SRC;
        auto depthResolveDesc    = depthDesc;
        depthResolveDesc.samples = SampleCount::X1;
        depthResolveDesc.usage |= ImageUsageFlagBit::SAMPLED;
        const std::array descriptors{colorDesc, depthDesc, colorResolveDesc, depthResolveDesc, colorResolveDesc};
        FrameGraphData   data;
        for (const auto &desc : descriptors) {
            const ImagePtr image(device->CreateImage(desc));
            ASSERT_TRUE(image);
            FrameGraphImage input;
            input.image      = FrameImageReference(image);
            input.descriptor = desc;
            input.initial    = ImageLayout::UNDEFINED;
            input.final      = ImageLayout::GENERAL;
            data.images.push_back(std::move(input));
        }
        Buffer::Descriptor bufferDesc;
        bufferDesc.size   = imageBytes * 2;
        bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
        bufferDesc.memory = MemoryType::CPU_TO_GPU;
        const BufferPtr readback(device->CreateBuffer(bufferDesc));
        ASSERT_TRUE(readback);
        data.buffers.push_back({readback, bufferDesc.size, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
        FrameRasterPass raster;
        raster.color          = {kGraphId, 0};
        raster.load           = LoadOp::CLEAR;
        raster.store          = store;
        raster.clear          = ClearValue(0.2f, 0.4f, 0.8f, 1);
        raster.colorResolve   = RasterColorResolve{{kGraphId, 2}, {}};
        raster.depth          = RasterDepthAttachment{{kGraphId, 1}, LoadOp::CLEAR, store, 0.25f, {}};
        raster.depth->resolve = RasterDepthResolve{{kGraphId, 3}, {}, DepthResolveMode::MINIMUM};
        data.passes.emplace_back(raster);
        if (store == StoreOp::STORE) {
            raster.load         = LoadOp::LOAD;
            raster.store        = StoreOp::DONT_CARE;
            raster.depth->load  = LoadOp::LOAD;
            raster.depth->store = StoreOp::DONT_CARE;
            data.passes.emplace_back(std::move(raster));
        }
        FrameRasterPass sample;
        sample.color = {kGraphId, 4};
        sample.load  = LoadOp::CLEAR;
        RasterDraw draw;
        draw.shader              = shader;
        draw.command.vertexCount = 3;
        RasterBindingDraft bindings;
        bindings.images   = {{0, 0, {kGraphId, 3}, {ImageViewType::VIEW_2D, 0, 1, 0, 1}}};
        bindings.samplers = {{0, 1, sampler}};
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
        sample.draws.push_back(std::move(draw));
        data.passes.emplace_back(std::move(sample));
        for (uint32_t i = 0; i < 2; ++i) {
            BufferImageCopyPass copy;
            copy.buffer                 = {kGraphId, 0};
            copy.image                  = {kGraphId, i ? 4u : 2u};
            copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
            copy.region.bufferOffset    = imageBytes * i;
            copy.region.imageExtent     = {kSize, kSize, 1};
            copy.region.subRange.layers = 1;
            data.passes.emplace_back(copy);
        }
        data.exports.push_back({{kGraphId, 0}, 0, bufferDesc.size});
        std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
        ASSERT_TRUE(context);
        const auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
        if (const auto *error = std::get_if<FrameError>(&compiled))
            FAIL() << error->message;
        auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        const auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        EXPECT_FALSE(plan);
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        ASSERT_TRUE(WaitForCompletion(*context));
        const auto *pixels = readback->Map();
        ASSERT_TRUE(pixels);
        for (uint32_t pixel = 0; pixel < kSize * kSize; ++pixel) {
            EXPECT_EQ(pixels[pixel * 4], 51u);
            EXPECT_EQ(pixels[pixel * 4 + 1], 102u);
            EXPECT_EQ(pixels[pixel * 4 + 2], 204u);
            EXPECT_EQ(pixels[pixel * 4 + 3], 255u);
            for (uint32_t channel = 0; channel < 3; ++channel)
                EXPECT_NEAR(pixels[imageBytes + pixel * 4 + channel], 64, 1);
            EXPECT_EQ(pixels[imageBytes + pixel * 4 + 3], 255u);
        }
        readback->UnMap();
    }
}

TEST_F(AuroraMetalTest, FrameContextClearsDepthAttachment)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor colorDesc;
    colorDesc.format = PixelFormat::RGBA8_UNORM;
    colorDesc.extent = {kSize, kSize, 1};
    colorDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    colorDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr color(device->CreateImage(colorDesc));
    ASSERT_NE(color, nullptr);

    Image::Descriptor depthDesc;
    depthDesc.format = PixelFormat::D32;
    depthDesc.extent = {kSize, kSize, 1};
    depthDesc.usage  = ImageUsageFlagBit::DEPTH_STENCIL;
    depthDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr depth(device->CreateImage(depthDesc));
    ASSERT_NE(depth, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage colorImage;
    colorImage.image      = FrameImageReference(color);
    colorImage.descriptor = colorDesc;
    colorImage.initial    = ImageLayout::UNDEFINED;
    colorImage.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(colorImage);

    FrameGraphImage depthImage;
    depthImage.image      = FrameImageReference(depth);
    depthImage.descriptor = depthDesc;
    depthImage.initial    = ImageLayout::UNDEFINED;
    depthImage.final      = ImageLayout::DEPTH_STENCIL_ATTACHMENT;
    data.images.push_back(depthImage);

    FrameRasterPass raster;
    raster.color = GraphImageHandle{kGraphId, 0};
    raster.load  = LoadOp::CLEAR;
    raster.clear = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    raster.depth = RasterDepthAttachment{GraphImageHandle{kGraphId, 1}, LoadOp::CLEAR, StoreOp::STORE, 1.0f, {}};
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                 = GraphBufferHandle{kGraphId, 0};
    copy.image                  = GraphImageHandle{kGraphId, 0};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = {kSize, kSize, 1};
    copy.region.subRange.layers = 1;
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 51);
    EXPECT_EQ(pixels[1], 102);
    EXPECT_EQ(pixels[2], 204);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextKeepsTwoSlotsInFlight)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imageDesc;
    imageDesc.format = PixelFormat::RGBA8_UNORM;
    imageDesc.extent = {kSize, kSize, 1};
    imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    auto makeReadback = [&]() {
        Buffer::Descriptor desc;
        desc.size   = kSize * kSize * 4;
        desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
        desc.memory = MemoryType::CPU_TO_GPU;
        return BufferPtr(device->CreateBuffer(desc));
    };
    auto makeGraph = [&](const BufferPtr &readback) {
        FrameGraphData   data;
        FrameGraphBuffer buffer;
        buffer.buffer  = readback;
        buffer.size    = kSize * kSize * 4;
        buffer.initial = CopyBufferState::COMMON;
        buffer.final   = CopyBufferState::HOST_READ;
        data.buffers.push_back(buffer);

        FrameGraphImage target;
        target.image      = FrameImageReference(image);
        target.descriptor = imageDesc;
        target.initial    = ImageLayout::UNDEFINED;
        target.final      = ImageLayout::TRANSFER_SRC;
        data.images.push_back(target);

        FrameRasterPass raster;
        raster.color = GraphImageHandle{kGraphId, 0};
        raster.load  = LoadOp::CLEAR;
        raster.clear = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
        data.passes.push_back(raster);

        BufferImageCopyPass copy;
        copy.buffer                 = GraphBufferHandle{kGraphId, 0};
        copy.image                  = GraphImageHandle{kGraphId, 0};
        copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
        copy.region.imageExtent     = {kSize, kSize, 1};
        copy.region.subRange.layers = 1;
        data.passes.push_back(copy);
        data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, kSize * kSize * 4});
        return std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    };

    BufferPtr readbacks[2] = {makeReadback(), makeReadback()};
    ASSERT_NE(readbacks[0], nullptr);
    ASSERT_NE(readbacks[1], nullptr);
    auto context = device->CreateFrameContext({2, 1, 1});
    ASSERT_NE(context, nullptr);

    for (uint32_t i = 0; i < 2; ++i) {
        auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), makeGraph(readbacks[i]));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    }

    size_t completed = 0;
    for (uint32_t i = 0; i < 2000 && completed < 2; ++i) {
        std::array<FrameCompletion, 2> completions{};
        const auto                     count = context->PollCompleted(completions);
        for (size_t c = 0; c < count; ++c)
            EXPECT_EQ(completions[c].status, FrameCompletionStatus::COMPLETE);
        completed += count;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(completed, 2u);

    for (auto &readback : readbacks) {
        auto *pixels = readback->Map();
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(pixels[0], 51);
        EXPECT_EQ(pixels[1], 102);
        EXPECT_EQ(pixels[2], 204);
        readback->UnMap();
    }
}

TEST_F(AuroraMetalTest, FrameContextRendersSelectedMipLayer)
{
    constexpr uint32_t baseSize = 16;
    auto              *device   = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imageDesc;
    imageDesc.format      = PixelFormat::RGBA8_UNORM;
    imageDesc.extent      = {baseSize, baseSize, 1};
    imageDesc.mipLevels   = 2;
    imageDesc.arrayLayers = 2;
    imageDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory      = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage target;
    target.image      = FrameImageReference(image);
    target.descriptor = imageDesc;
    target.initial    = ImageLayout::UNDEFINED;
    target.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(target);

    FrameRasterPass raster;
    raster.color     = GraphImageHandle{kGraphId, 0};
    raster.colorView = RasterAttachmentView{1, 1};
    raster.load      = LoadOp::CLEAR;
    raster.clear     = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                    = GraphBufferHandle{kGraphId, 0};
    copy.image                     = GraphImageHandle{kGraphId, 0};
    copy.direction                 = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.subRange.level     = 1;
    copy.region.subRange.baseLayer = 1;
    copy.region.subRange.layers    = 1;
    copy.region.imageExtent        = {kSize, kSize, 1};
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 51);
    EXPECT_EQ(pixels[1], 102);
    EXPECT_EQ(pixels[2], 204);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextResolvesDepth)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor colorDesc;
    colorDesc.format  = PixelFormat::RGBA8_UNORM;
    colorDesc.extent  = {kSize, kSize, 1};
    colorDesc.usage   = ImageUsageFlagBit::RENDER_TARGET;
    colorDesc.samples = SampleCount::X4;
    colorDesc.memory  = MemoryType::GPU_ONLY;
    ImagePtr color(device->CreateImage(colorDesc));
    ASSERT_NE(color, nullptr);

    Image::Descriptor depthDesc;
    depthDesc.format  = PixelFormat::D32;
    depthDesc.extent  = {kSize, kSize, 1};
    depthDesc.samples = SampleCount::X4;
    depthDesc.usage   = ImageUsageFlagBit::DEPTH_STENCIL;
    depthDesc.memory  = MemoryType::GPU_ONLY;
    ImagePtr depth(device->CreateImage(depthDesc));
    ASSERT_NE(depth, nullptr);

    Image::Descriptor resolveDesc;
    resolveDesc.format = PixelFormat::D32;
    resolveDesc.extent = {kSize, kSize, 1};
    resolveDesc.usage  = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::TRANSFER_SRC;
    resolveDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr resolved(device->CreateImage(resolveDesc));
    ASSERT_NE(resolved, nullptr);

    Image::Descriptor colorResolveDesc;
    colorResolveDesc.format = PixelFormat::RGBA8_UNORM;
    colorResolveDesc.extent = {kSize, kSize, 1};
    colorResolveDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    colorResolveDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr colorResolved(device->CreateImage(colorResolveDesc));
    ASSERT_NE(colorResolved, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage colorImage;
    colorImage.image      = FrameImageReference(color);
    colorImage.descriptor = colorDesc;
    colorImage.initial    = ImageLayout::UNDEFINED;
    colorImage.final      = ImageLayout::COLOR_ATTACHMENT;
    data.images.push_back(colorImage);

    FrameGraphImage depthImage;
    depthImage.image      = FrameImageReference(depth);
    depthImage.descriptor = depthDesc;
    depthImage.initial    = ImageLayout::UNDEFINED;
    depthImage.final      = ImageLayout::DEPTH_STENCIL_ATTACHMENT;
    data.images.push_back(depthImage);

    FrameGraphImage resolvedImage;
    resolvedImage.image      = FrameImageReference(resolved);
    resolvedImage.descriptor = resolveDesc;
    resolvedImage.initial    = ImageLayout::UNDEFINED;
    resolvedImage.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(resolvedImage);

    FrameGraphImage colorResolvedImage;
    colorResolvedImage.image      = FrameImageReference(colorResolved);
    colorResolvedImage.descriptor = colorResolveDesc;
    colorResolvedImage.initial    = ImageLayout::UNDEFINED;
    colorResolvedImage.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(colorResolvedImage);

    FrameRasterPass raster;
    raster.color        = GraphImageHandle{kGraphId, 0};
    raster.load         = LoadOp::CLEAR;
    raster.clear        = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    raster.colorResolve = RasterColorResolve{GraphImageHandle{kGraphId, 3}, {}};
    RasterDepthAttachment depthAttachment{GraphImageHandle{kGraphId, 1}, LoadOp::CLEAR, StoreOp::STORE, 0.5f, {}};
    depthAttachment.resolve = RasterDepthResolve{GraphImageHandle{kGraphId, 2}, {}, DepthResolveMode::MINIMUM};
    raster.depth            = depthAttachment;
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                 = GraphBufferHandle{kGraphId, 0};
    copy.image                  = GraphImageHandle{kGraphId, 3};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = {kSize, kSize, 1};
    copy.region.subRange.layers = 1;
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    if (auto *error = std::get_if<FrameError>(&compiled))
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 51);
    EXPECT_EQ(pixels[1], 102);
    EXPECT_EQ(pixels[2], 204);
    readback->UnMap();
}

TEST_F(AuroraMetalTest, FrameContextReusesSlotsAcrossFrames)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imageDesc;
    imageDesc.format = PixelFormat::RGBA8_UNORM;
    imageDesc.extent = {kSize, kSize, 1};
    imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer buffer;
    buffer.buffer  = readback;
    buffer.size    = bufferDesc.size;
    buffer.initial = CopyBufferState::COMMON;
    buffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(buffer);

    FrameGraphImage target;
    target.image      = FrameImageReference(image);
    target.descriptor = imageDesc;
    target.initial    = ImageLayout::UNDEFINED;
    target.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(target);

    FrameRasterPass raster;
    raster.color = GraphImageHandle{kGraphId, 0};
    raster.load  = LoadOp::CLEAR;
    raster.clear = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    data.passes.push_back(raster);

    BufferImageCopyPass copy;
    copy.buffer                 = GraphBufferHandle{kGraphId, 0};
    copy.image                  = GraphImageHandle{kGraphId, 0};
    copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.imageExtent     = {kSize, kSize, 1};
    copy.region.subRange.layers = 1;
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});
    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));

    auto context = device->CreateFrameContext({2, 1, 1});
    ASSERT_NE(context, nullptr);
    for (uint32_t frame = 0; frame < 32; ++frame) {
        auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun)) << "frame " << frame;
        auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled)) << "frame " << frame;
        auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        ASSERT_TRUE(WaitForCompletion(*context)) << "frame " << frame;

        auto *pixels = readback->Map();
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(pixels[0], 51) << "frame " << frame;
        EXPECT_EQ(pixels[1], 102) << "frame " << frame;
        EXPECT_EQ(pixels[2], 204) << "frame " << frame;
        readback->UnMap();
    }
}

TEST_F(AuroraMetalTest, FrameContextSamplesImageThroughView)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    constexpr uint32_t texSize = 8;
    Image::Descriptor  sourceDesc;
    sourceDesc.format = PixelFormat::RGBA8_UNORM;
    sourceDesc.extent = {texSize, texSize, 1};
    sourceDesc.usage  = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    sourceDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr source(device->CreateImage(sourceDesc));
    ASSERT_NE(source, nullptr);

    Buffer::Descriptor uploadDesc;
    uploadDesc.size   = texSize * texSize * 4;
    uploadDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    uploadDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr upload(device->CreateBuffer(uploadDesc));
    ASSERT_NE(upload, nullptr);
    auto *uploadPixels = upload->Map();
    ASSERT_NE(uploadPixels, nullptr);
    for (uint32_t i = 0; i < texSize * texSize; ++i) {
        uploadPixels[i * 4 + 0] = 0;
        uploadPixels[i * 4 + 1] = 255;
        uploadPixels[i * 4 + 2] = 0;
        uploadPixels[i * 4 + 3] = 255;
    }
    upload->UnMap();

    Image::Descriptor colorDesc;
    colorDesc.format = PixelFormat::RGBA8_UNORM;
    colorDesc.extent = {kSize, kSize, 1};
    colorDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    colorDesc.memory = MemoryType::GPU_ONLY;
    ImagePtr color(device->CreateImage(colorDesc));
    ASSERT_NE(color, nullptr);

    Buffer::Descriptor readbackDesc;
    readbackDesc.size   = kSize * kSize * 4;
    readbackDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    readbackDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(readbackDesc));
    ASSERT_NE(readback, nullptr);

    SamplerPtr sampler(device->CreateSampler({}));
    ASSERT_NE(sampler, nullptr);

    FrameGraphData   data;
    FrameGraphBuffer uploadBuffer;
    uploadBuffer.buffer      = upload;
    uploadBuffer.size        = uploadDesc.size;
    uploadBuffer.initial     = CopyBufferState::COMMON;
    uploadBuffer.final       = CopyBufferState::COPY_SOURCE;
    uploadBuffer.initialized = true;
    data.buffers.push_back(uploadBuffer);

    FrameGraphBuffer readbackBuffer;
    readbackBuffer.buffer  = readback;
    readbackBuffer.size    = readbackDesc.size;
    readbackBuffer.initial = CopyBufferState::COMMON;
    readbackBuffer.final   = CopyBufferState::HOST_READ;
    data.buffers.push_back(readbackBuffer);

    FrameGraphImage sourceImage;
    sourceImage.image      = FrameImageReference(source);
    sourceImage.descriptor = sourceDesc;
    sourceImage.initial    = ImageLayout::UNDEFINED;
    sourceImage.final      = ImageLayout::SHADER_READ_ONLY;
    data.images.push_back(sourceImage);

    FrameGraphImage colorImage;
    colorImage.image      = FrameImageReference(color);
    colorImage.descriptor = colorDesc;
    colorImage.initial    = ImageLayout::UNDEFINED;
    colorImage.final      = ImageLayout::TRANSFER_SRC;
    data.images.push_back(colorImage);

    BufferImageCopyPass uploadPass;
    uploadPass.buffer                 = GraphBufferHandle{kGraphId, 0};
    uploadPass.image                  = GraphImageHandle{kGraphId, 0};
    uploadPass.direction              = ImageCopyDirection::BUFFER_TO_IMAGE;
    uploadPass.region.imageExtent     = {texSize, texSize, 1};
    uploadPass.region.subRange.layers = 1;
    data.passes.push_back(uploadPass);

    const char      *samplingFragment = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 position [[position]]; };
vertex VOut VSMain(uint vid [[vertex_id]]) {
    const float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    VOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    return out;
}
fragment float4 FSMain(VOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler sam [[sampler(0)]]) {
    return tex.sample(sam, float2(0.5, 0.5));
}
)MSL";
    ShaderReflection samplingReflection;
    ShaderResource   sampledResource;
    sampledResource.name               = "tex";
    sampledResource.type               = ShaderResourceType::SAMPLED_IMAGE;
    sampledResource.set                = 0;
    sampledResource.binding            = 0;
    sampledResource.count              = 1;
    sampledResource.texture.dimension  = ShaderTextureDimension::D2;
    sampledResource.texture.scalarType = ShaderScalarType::FLOAT;
    sampledResource.texture.components = 4;
    samplingReflection.resources.push_back(sampledResource);
    ShaderResource samplerResource;
    samplerResource.name    = "sam";
    samplerResource.type    = ShaderResourceType::SAMPLER;
    samplerResource.set     = 0;
    samplerResource.binding = 1;
    samplerResource.count   = 1;
    samplerResource.sampler = ShaderSamplerType::REGULAR;
    samplingReflection.resources.push_back(samplerResource);
    for (auto &resource : samplingReflection.resources)
        resource.stages = ShaderStageFlagBit::FS;
    samplingReflection.nativeBindings = {{0, 0, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_TEXTURE},
                                         {0, 1, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_SAMPLER}};

    auto shader = MakeFullscreenShader(device, samplingFragment, &samplingReflection);
    ASSERT_NE(shader, nullptr);

    FrameRasterPass raster;
    raster.color = GraphImageHandle{kGraphId, 1};
    raster.load  = LoadOp::CLEAR;
    raster.clear = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    RasterDraw draw;
    draw.shader  = shader;
    draw.command = {3, 1, 0, 0};
    RasterBindingDraft bindings;
    bindings.images.push_back(RasterImageBinding{0, 0, GraphImageHandle{kGraphId, 0}, RasterImageView{}});
    bindings.samplers.push_back(RasterSamplerBinding{0, 1, sampler});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
    raster.draws.push_back(draw);
    data.passes.push_back(raster);

    BufferImageCopyPass readbackPass;
    readbackPass.buffer                 = GraphBufferHandle{kGraphId, 1};
    readbackPass.image                  = GraphImageHandle{kGraphId, 1};
    readbackPass.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
    readbackPass.region.imageExtent     = {kSize, kSize, 1};
    readbackPass.region.subRange.layers = 1;
    data.passes.push_back(readbackPass);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 1}, 0, readbackDesc.size});

    auto graph   = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    if (auto *error = std::get_if<FrameError>(&compiled))
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));

    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 0);
    EXPECT_EQ(pixels[1], 255);
    EXPECT_EQ(pixels[2], 0);
    EXPECT_EQ(pixels[3], 255);
    readback->UnMap();
}
TEST_F(AuroraMetalTest, RasterCompileRejectsFloatSamplerWithUintImageWithoutSubmitting)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    Image::Descriptor sourceDesc;
    sourceDesc.format = PixelFormat::RGBA32_UINT;
    sourceDesc.extent = {kSize, kSize, 1};
    sourceDesc.usage  = ImageUsageFlagBit::SAMPLED;
    ImagePtr source(device->CreateImage(sourceDesc));
    ASSERT_TRUE(source);
    auto colorDesc   = sourceDesc;
    colorDesc.format = PixelFormat::RGBA8_UNORM;
    colorDesc.usage  = ImageUsageFlagBit::RENDER_TARGET;
    ImagePtr   color(device->CreateImage(colorDesc));
    SamplerPtr sampler(device->CreateSampler({}));
    ASSERT_TRUE(color && sampler);

    ShaderReflection reflection;
    ShaderResource   texture;
    texture.name    = "tex";
    texture.type    = ShaderResourceType::SAMPLED_IMAGE;
    texture.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
    texture.stages  = ShaderStageFlagBit::FS;
    reflection.resources.push_back(texture);
    ShaderResource sampled;
    sampled.name    = "sam";
    sampled.type    = ShaderResourceType::SAMPLER;
    sampled.binding = 1;
    sampled.sampler = ShaderSamplerType::REGULAR;
    sampled.stages  = ShaderStageFlagBit::FS;
    reflection.resources.push_back(sampled);
    reflection.nativeBindings = {{0, 0, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_TEXTURE},
                                 {0, 1, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_SAMPLER}};
    auto shader               = MakeFullscreenShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 position [[position]]; };
vertex VOut VSMain(uint vertexId [[vertex_id]]) {
    const float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    return {float4(positions[vertexId], 0, 1)};
}
fragment float4 FSMain(VOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler sam [[sampler(0)]]) {
    return tex.sample(sam, float2(0.5));
}
)MSL",
                                                     &reflection);
    ASSERT_TRUE(shader);
    FrameGraphData  data;
    FrameGraphImage input;
    input.image       = FrameImageReference(source);
    input.descriptor  = sourceDesc;
    input.initial     = ImageLayout::SHADER_READ_ONLY;
    input.final       = ImageLayout::SHADER_READ_ONLY;
    input.initialized = true;
    data.images.push_back(input);
    FrameGraphImage output;
    output.image      = FrameImageReference(color);
    output.descriptor = colorDesc;
    output.final      = ImageLayout::COLOR_ATTACHMENT;
    data.images.push_back(output);
    FrameRasterPass raster;
    raster.color      = {kGraphId, 1};
    raster.sideEffect = true;
    raster.load       = LoadOp::CLEAR;
    RasterDraw draw;
    draw.shader  = shader;
    draw.command = {3, 1, 0, 0};
    RasterBindingDraft bindings;
    bindings.images.push_back({0, 0, {kGraphId, 0}, {}});
    bindings.samplers.push_back({0, 1, sampler});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
    raster.draws.push_back(std::move(draw));
    data.passes.push_back(std::move(raster));

    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_TRUE(context);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto lease    = std::get<DeviceFrameLease>(begun);
    const auto compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
    EXPECT_FALSE(std::get<FrameError>(compiled).message.empty());
    std::array<FrameCompletion, 1> completed;
    EXPECT_EQ(context->PollCompleted(completed), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto next = std::get<DeviceFrameLease>(begun);
    EXPECT_EQ(next.slot, lease.slot);
    EXPECT_GT(next.generation, lease.generation);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
}

TEST_F(AuroraMetalTest, ImageCreationHonorsCompressedAndMultisampleCapabilities)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    Image::Descriptor desc;
    desc.extent = {4, 4, 1};
    desc.usage  = ImageUsageFlagBit::SAMPLED;
    for (const auto format : {PixelFormat::BC1_RGBA_UNORM_BLOCK, PixelFormat::BC7_UNORM_BLOCK}) {
        desc.format         = format;
        const auto features = device->GetFormatFeatureFlags(format);
        ImagePtr   image(device->CreateImage(desc));
        EXPECT_EQ(image != nullptr, features.TestBit(PixelFormatFeatureFlagBit::SAMPLE));
        EXPECT_FALSE(features.TestBit(PixelFormatFeatureFlagBit::COLOR));
        desc.samples = SampleCount::X4;
        EXPECT_EQ(device->CreateImage(desc), nullptr);
        desc.samples = SampleCount::X1;
    }
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.samples     = SampleCount::X4;
    desc.mipLevels   = 2;
    desc.arrayLayers = 2;
    desc.usage       = ImageUsageFlagBit::RENDER_TARGET;
    EXPECT_EQ(device->CreateImage(desc), nullptr);
}

TEST_F(AuroraMetalTest, DepthOnlyArraySliceIsSampledAfterLoad)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    Image::Descriptor colorDesc;
    colorDesc.format = PixelFormat::RGBA8_UNORM;
    colorDesc.extent = {kSize, kSize, 1};
    colorDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    ImagePtr color(device->CreateImage(colorDesc));
    ASSERT_NE(color, nullptr);
    BufferPtr readback(device->CreateBuffer({kSize * kSize * 4, BufferUsageFlagBit::TRANSFER_DST, MemoryType::CPU_TO_GPU}));
    ASSERT_NE(readback, nullptr);
    auto            data = BuildClearReadbackGraph(color, colorDesc, readback)->GetData();
    FrameGraphImage depth;
    depth.transient            = true;
    depth.descriptor.extent    = {16, 16, 1};
    depth.descriptor.mipLevels = depth.descriptor.arrayLayers = 2;
    depth.descriptor.format                                   = PixelFormat::D32;
    depth.descriptor.usage                                    = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::SAMPLED;
    depth.final                                               = ImageLayout::GENERAL;
    data.images.push_back(depth);
    auto depthShader = MakeFullscreenShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 VSMain(uint id [[vertex_id]]) {
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * 2 - 1, 0.25, 1);
}
fragment void FSMain() {}
)MSL");
    ASSERT_NE(depthShader, nullptr);
    FrameRasterPass write;
    write.depth = RasterDepthAttachment{{kGraphId, 1}, LoadOp::CLEAR, StoreOp::STORE, 1, {1, 1}};
    RasterDraw draw;
    draw.shader              = depthShader;
    draw.command.vertexCount = 3;
    draw.depth               = {true, true, CompareOp::LESS_OR_EQUAL};
    draw.scissor             = {{0, 0}, {4, 8}};
    write.draws.push_back(draw);
    auto load        = write;
    load.depth->load = LoadOp::LOAD;
    load.draws.clear();
    ShaderReflection reflection;
    ShaderResource   image;
    image.name    = "depth";
    image.stages  = ShaderStageFlagBit::FS;
    image.type    = ShaderResourceType::SAMPLED_IMAGE;
    image.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 1, true, false};
    reflection.resources.push_back(image);
    ShaderResource samplerResource;
    samplerResource.type    = ShaderResourceType::SAMPLER;
    samplerResource.binding = 1;
    samplerResource.sampler = ShaderSamplerType::REGULAR;
    samplerResource.name    = "sam";
    samplerResource.stages  = ShaderStageFlagBit::FS;
    reflection.resources.push_back(samplerResource);
    reflection.nativeBindings = {{0, 0, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_TEXTURE},
                                 {0, 1, ShaderStageFlagBit::FS, 0, ShaderNativeCategory::METAL_SAMPLER}};
    auto sampleShader         = MakeFullscreenShader(device, R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 VSMain(uint id [[vertex_id]]) {
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * 2 - 1, 0, 1);
}
fragment float4 FSMain(float4 p [[position]], depth2d_array<float> depth [[texture(0)]], sampler sam [[sampler(0)]]) {
    float d = depth.sample(sam, p.xy / 8, 0, level(0));
    return float4(d, d, d, 1);
}
)MSL",
                                                     &reflection);
    ASSERT_NE(sampleShader, nullptr);
    Sampler::Descriptor samplerDesc;
    samplerDesc.minFilter = samplerDesc.magFilter = Filter::NEAREST;
    samplerDesc.mipmapMode                        = MipFilter::NEAREST;
    SamplerPtr sampler(device->CreateSampler(samplerDesc));
    ASSERT_NE(sampler, nullptr);
    draw                     = {};
    draw.shader              = sampleShader;
    draw.command.vertexCount = 3;
    RasterBindingDraft bindings;
    bindings.images   = {{0, 0, {kGraphId, 1}, {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}}};
    bindings.samplers = {{0, 1, sampler}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(bindings))));
    std::get<FrameRasterPass>(data.passes[0]).draws.push_back(draw);
    const auto repeatedSample = data.passes[0];
    auto       repeatedCopy   = std::get<BufferImageCopyPass>(data.passes[1]);
    data.passes.insert(data.passes.begin(), {FrameGraphPass{write}, FrameGraphPass{load}});
    const BufferPtr rewrittenReadback(device->CreateBuffer({kSize * kSize * 4, BufferUsageFlagBit::TRANSFER_DST, MemoryType::CPU_TO_GPU}));
    ASSERT_TRUE(rewrittenReadback);
    data.buffers.push_back({rewrittenReadback, kSize * kSize * 4, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
    write.draws.clear();
    write.depth->clear = 0.75f;
    data.passes.emplace_back(std::move(write));
    data.passes.push_back(repeatedSample);
    repeatedCopy.buffer = {kGraphId, 1};
    data.passes.emplace_back(repeatedCopy);
    data.exports.push_back({{kGraphId, 1}, 0, kSize * kSize * 4});
    auto context = device->CreateFrameContext({1, 1, 1});
    ASSERT_NE(context, nullptr);
    auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
    if (const auto *error = std::get_if<FrameError>(&compiled))
        FAIL() << error->message;
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    ASSERT_TRUE(WaitForCompletion(*context));
    auto *pixels = readback->Map();
    ASSERT_NE(pixels, nullptr);
    EXPECT_NEAR(pixels[(3 * 8 + 1) * 4], 64, 1);
    EXPECT_EQ(pixels[(3 * 8 + 6) * 4], 255);
    EXPECT_EQ(pixels[(3 * 8 + 1) * 4 + 3], 255);
    EXPECT_EQ(pixels[(3 * 8 + 6) * 4 + 3], 255);
    readback->UnMap();
    pixels = rewrittenReadback->Map();
    ASSERT_NE(pixels, nullptr);
    for (uint32_t pixel = 0; pixel < kSize * kSize; ++pixel) {
        for (uint32_t channel = 0; channel < 3; ++channel)
            EXPECT_NEAR(pixels[pixel * 4 + channel], 191, 1);
        EXPECT_EQ(pixels[pixel * 4 + 3], 255u);
    }
    rewrittenReadback->UnMap();
}
#endif
