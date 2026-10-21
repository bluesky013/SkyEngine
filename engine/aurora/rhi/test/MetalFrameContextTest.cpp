#include "AuroraTestHelper.h"

#include <array>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/Shader.h>
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

        auto makeFunction = [&](ShaderStageFlagBit stage) {
            auto data = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(std::strlen(source))));
            std::memcpy(data->Data(), source, std::strlen(source));
            auto provider        = CounterPtr<ShaderBinaryProvider>(new ShaderBinaryProvider);
            provider->binaryData = data;
            ShaderFunction::Descriptor desc{stage, provider, {}};
            return ShaderFunctionPtr(device->CreateShaderFunction(desc));
        };

        auto vs = makeFunction(ShaderStageFlagBit::VS);
        auto fs = makeFunction(ShaderStageFlagBit::FS);
        if (!vs || !fs)
            return {};
        ShaderReflection reflection;
        if (reflectionOverride != nullptr)
            reflection = *reflectionOverride;
        Shader::Descriptor desc{};
        desc.vs         = vs.Get();
        desc.ps         = fs.Get();
        desc.reflection = &reflection;
        return ShaderPtr(device->CreateShader(desc));
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

TEST_F(AuroraMetalTest, FrameContextComputeUsesDynamicOffsetsInBindingOrder)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char                      *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { uint value; };
kernel void CSMain(constant Params &first [[buffer(0)]], constant Params &second [[buffer(1)]],
                   device uint *output [[buffer(2)]], uint id [[thread_position_in_grid]]) {
    output[id] = first.value + 10 * second.value + id;
}
)MSL";
    CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
    provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(std::strlen(source))));
    std::memcpy(provider->binaryData->Data(), source, std::strlen(source));
    const auto function = ShaderFunctionPtr(device->CreateShaderFunction({ShaderStageFlagBit::CS, provider, "CSMain"}));
    ASSERT_TRUE(function);
    ShaderReflection reflection;
    for (const auto binding : {1u, 3u}) {
        ShaderResource uniform;
        uniform.type    = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        uniform.binding = binding;
        reflection.resources.push_back(uniform);
    }
    ShaderResource storage;
    storage.type = ShaderResourceType::STORAGE_BUFFER;
    storage.set  = 1;
    reflection.resources.push_back(storage);
    reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
    Shader::Descriptor shaderDesc{};
    shaderDesc.cs         = function.Get();
    shaderDesc.reflection = &reflection;
    auto shader           = ShaderPtr(device->CreateShader(shaderDesc));
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
    compute.buffers.push_back({0, 3, {{kGraphId, 0}, 0, 16}, 512, FrameAccessMode::READ});
    compute.buffers.push_back({0, 1, {{kGraphId, 0}, 0, 16}, 256, FrameAccessMode::READ});
    compute.buffers.push_back({1, 0, {{kGraphId, 1}, 0, 16}, 0, FrameAccessMode::WRITE});
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
    const auto result = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(result));
    EXPECT_EQ(std::get<EncodingFailed>(result).error.code, FrameErrorCode::ENCODE_FAILED);
    EXPECT_EQ(first->calls, 1u);
    EXPECT_EQ(second->calls, 0u);
    plan.reset();
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

TEST_F(AuroraMetalTest, ComputeInputValidationPreservesLease)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char                      *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void CSMain(constant uint &input [[buffer(0)]], device uint *output [[buffer(1)]], uint id [[thread_position_in_grid]]) {
    output[id] = input + id;
}
)MSL";
    CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
    provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(std::strlen(source))));
    std::memcpy(provider->binaryData->Data(), source, std::strlen(source));
    const auto function = ShaderFunctionPtr(device->CreateShaderFunction({ShaderStageFlagBit::CS, provider, "CSMain"}));
    ASSERT_TRUE(function);
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    for (uint32_t fault = 0; fault < 10; ++fault) {
        SCOPED_TRACE(fault);
        ShaderReflection reflection;
        ShaderResource   uniform;
        uniform.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        reflection.resources.push_back(uniform);
        ShaderResource storage;
        storage.type    = ShaderResourceType::STORAGE_BUFFER;
        storage.binding = 1;
        reflection.resources.push_back(storage);
        reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        if (fault == 6)
            reflection.threadGroupSize[0] = 0;
        if (fault == 7)
            reflection.threadGroupSize[0] = UINT32_MAX;
        if (fault == 9)
            reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = 1024;
        Shader::Descriptor shaderDesc{};
        shaderDesc.cs         = function.Get();
        shaderDesc.reflection = &reflection;
        auto shader           = fault == 0 ? MakeFullscreenShader(device, nullptr, &reflection) : ShaderPtr(device->CreateShader(shaderDesc));
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
        pass.buffers.push_back({0, 0, {{kGraphId, 0}, fault == 2 ? 4u : 0u, 16}, fault == 3 ? 4u : (fault == 5 ? 512u : 0u), FrameAccessMode::READ});
        pass.buffers.push_back({0, 1, {{kGraphId, 1}, 0, 16}, 0, FrameAccessMode::WRITE});
        pass.sideEffect = true;
        data.passes.emplace_back(std::move(pass));
        const auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        const auto lease    = std::get<DeviceFrameLease>(begun);
        const auto compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
        EXPECT_EQ(std::get<FrameError>(compiled).code, fault == 6 ? FrameErrorCode::UNSUPPORTED : FrameErrorCode::INVALID_ARGUMENT);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_F(AuroraMetalTest, StorageDynamicOffsetsUseNativeTypeAlignment)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    const char                      *source = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void CSMain(device const uint *input [[buffer(0)]], device uint *output [[buffer(1)]]) {
    output[0] = input[0];
}
)MSL";
    CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
    provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(std::strlen(source))));
    std::memcpy(provider->binaryData->Data(), source, std::strlen(source));
    const auto function = ShaderFunctionPtr(device->CreateShaderFunction({ShaderStageFlagBit::CS, provider, "CSMain"}));
    ASSERT_TRUE(function);
    ShaderReflection reflection;
    ShaderResource   inputResource;
    inputResource.type = ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
    ShaderResource outputResource;
    outputResource.type           = ShaderResourceType::STORAGE_BUFFER;
    outputResource.binding        = 1;
    reflection.resources          = {inputResource, outputResource};
    reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
    Shader::Descriptor shaderDesc{};
    shaderDesc.cs         = function.Get();
    shaderDesc.reflection = &reflection;
    const auto shader     = ShaderPtr(device->CreateShader(shaderDesc));
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
        pass.buffers = {{0, 0, {{kGraphId, 0}, 4, 4}, offset, FrameAccessMode::READ}, {0, 1, {{kGraphId, 1}, 0, 4}, 0, FrameAccessMode::WRITE}};
        data.passes.emplace_back(std::move(pass));
        data.exports.push_back({{kGraphId, 1}, 0, 4});
        auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        const auto lease    = std::get<DeviceFrameLease>(begun);
        auto       compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data)));
        if (offset == 1) {
            ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
            EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_ARGUMENT);
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

    auto shader = MakeFullscreenShader(device, samplingFragment, &samplingReflection);
    ASSERT_NE(shader, nullptr);

    FrameRasterPass raster;
    raster.color = GraphImageHandle{kGraphId, 1};
    raster.load  = LoadOp::CLEAR;
    raster.clear = ClearValue(0.2f, 0.4f, 0.8f, 1.0f);
    RasterDraw draw;
    draw.shader  = shader;
    draw.command = {3, 1, 0, 0};
    draw.images.push_back(RasterImageBinding{0, 0, GraphImageHandle{kGraphId, 0}, RasterImageView{}});
    draw.samplers.push_back(RasterSamplerBinding{0, 1, sampler});
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
    image.type    = ShaderResourceType::SAMPLED_IMAGE;
    image.texture = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 1, true, false};
    reflection.resources.push_back(image);
    ShaderResource samplerResource;
    samplerResource.type    = ShaderResourceType::SAMPLER;
    samplerResource.binding = 1;
    samplerResource.sampler = ShaderSamplerType::REGULAR;
    reflection.resources.push_back(samplerResource);
    auto sampleShader = MakeFullscreenShader(device, R"MSL(
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
    draw.images              = {{0, 0, {kGraphId, 1}, {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}}};
    draw.samplers            = {{0, 1, sampler}};
    std::get<FrameRasterPass>(data.passes[0]).draws.push_back(draw);
    data.passes.insert(data.passes.begin(), {FrameGraphPass{write}, FrameGraphPass{load}});
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
    readback->UnMap();
}
#endif
