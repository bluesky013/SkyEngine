#include "AuroraTestHelper.h"

#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/Shader.h>
#include <core/archive/BinaryData.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <chrono>
#include <cstring>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
namespace {
    constexpr uint32_t kSize = 8;
    constexpr uint64_t kGraphId = 1;

    std::shared_ptr<const FrozenFrameGraph> BuildClearReadbackGraph(const ImagePtr &image, const Image::Descriptor &imageDesc, const BufferPtr &readback)
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
        copy.buffer                              = GraphBufferHandle{kGraphId, 0};
        copy.image                               = GraphImageHandle{kGraphId, 0};
        copy.direction                           = ImageCopyDirection::IMAGE_TO_BUFFER;
        copy.region.imageExtent                  = {kSize, kSize, 1};
        copy.region.subRange.layers              = 1;
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
            auto provider = CounterPtr<ShaderBinaryProvider>(new ShaderBinaryProvider);
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

    auto graph = BuildClearReadbackGraph(image, imageDesc, readback);
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

    FrameGraphData data;
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

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
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

TEST_F(AuroraMetalTest, FrameContextRejectsUnsupportedPasses)
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
    EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::UNSUPPORTED);
}

TEST_F(AuroraMetalTest, FrameContextResolvesMsaa)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor msaaDesc;
    msaaDesc.format      = PixelFormat::RGBA8_UNORM;
    msaaDesc.extent      = {kSize, kSize, 1};
    msaaDesc.samples     = SampleCount::X4;
    msaaDesc.usage       = ImageUsageFlagBit::RENDER_TARGET;
    msaaDesc.memory      = MemoryType::GPU_ONLY;
    ImagePtr msaa(device->CreateImage(msaaDesc));
    ASSERT_NE(msaa, nullptr);

    Image::Descriptor resolveDesc;
    resolveDesc.format      = PixelFormat::RGBA8_UNORM;
    resolveDesc.extent      = {kSize, kSize, 1};
    resolveDesc.samples     = SampleCount::X1;
    resolveDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    resolveDesc.memory      = MemoryType::GPU_ONLY;
    ImagePtr resolve(device->CreateImage(resolveDesc));
    ASSERT_NE(resolve, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData data;
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

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
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
    colorDesc.format    = PixelFormat::RGBA8_UNORM;
    colorDesc.extent    = {kSize, kSize, 1};
    colorDesc.usage     = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    colorDesc.memory    = MemoryType::GPU_ONLY;
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

    FrameGraphData data;
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

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
    imageDesc.format    = PixelFormat::RGBA8_UNORM;
    imageDesc.extent    = {kSize, kSize, 1};
    imageDesc.usage     = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory    = MemoryType::GPU_ONLY;
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
        auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
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
    auto *device = GetDevice();
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

    FrameGraphData data;
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
    copy.buffer                     = GraphBufferHandle{kGraphId, 0};
    copy.image                      = GraphImageHandle{kGraphId, 0};
    copy.direction                  = ImageCopyDirection::IMAGE_TO_BUFFER;
    copy.region.subRange.level      = 1;
    copy.region.subRange.baseLayer  = 1;
    copy.region.subRange.layers     = 1;
    copy.region.imageExtent         = {kSize, kSize, 1};
    data.passes.push_back(copy);
    data.exports.push_back(GraphBufferExport{GraphBufferHandle{kGraphId, 0}, 0, bufferDesc.size});

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
    colorDesc.format = PixelFormat::RGBA8_UNORM;
    colorDesc.extent = {kSize, kSize, 1};
    colorDesc.usage  = ImageUsageFlagBit::RENDER_TARGET;
    colorDesc.samples = SampleCount::X4;
    colorDesc.memory = MemoryType::GPU_ONLY;
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

    FrameGraphData data;
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
    raster.color = GraphImageHandle{kGraphId, 0};
    raster.load  = LoadOp::CLEAR;
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

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
    imageDesc.format    = PixelFormat::RGBA8_UNORM;
    imageDesc.extent    = {kSize, kSize, 1};
    imageDesc.usage     = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imageDesc.memory    = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(imageDesc));
    ASSERT_NE(image, nullptr);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size   = kSize * kSize * 4;
    bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    bufferDesc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr readback(device->CreateBuffer(bufferDesc));
    ASSERT_NE(readback, nullptr);

    FrameGraphData data;
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
        auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
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
    Image::Descriptor sourceDesc;
    sourceDesc.format    = PixelFormat::RGBA8_UNORM;
    sourceDesc.extent    = {texSize, texSize, 1};
    sourceDesc.usage     = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    sourceDesc.memory    = MemoryType::GPU_ONLY;
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

    FrameGraphData data;
    FrameGraphBuffer uploadBuffer;
    uploadBuffer.buffer  = upload;
    uploadBuffer.size    = uploadDesc.size;
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

    const char *samplingFragment = R"MSL(
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
    ShaderResource    sampledResource;
    sampledResource.name                       = "tex";
    sampledResource.type                       = ShaderResourceType::SAMPLED_IMAGE;
    sampledResource.set                        = 0;
    sampledResource.binding                    = 0;
    sampledResource.count                      = 1;
    sampledResource.texture.dimension           = ShaderTextureDimension::D2;
    sampledResource.texture.scalarType          = ShaderScalarType::FLOAT;
    sampledResource.texture.components          = 4;
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
    draw.shader = shader;
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

    auto graph = std::make_shared<const FrozenFrameGraph>(kGraphId, std::move(data));
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
#endif








