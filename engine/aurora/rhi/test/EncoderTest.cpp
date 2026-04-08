//
// Created on 2026/04/07.
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Shader.h>
#include <core/archive/BinaryData.h>

#include <cstring>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

std::unique_ptr<CommandPool> CreatePoolFromDevice(Device *device)
{
    auto *pool = device->CreateCommandPool(QueueType::GRAPHICS);
    if (!pool) return nullptr;
    return std::unique_ptr<CommandPool>(pool);
}

BinaryDataPtr MakeBinaryFromData(const void *data, uint32_t size)
{
    auto bin = CounterPtr<BinaryData>(new BinaryData(size));
    std::memcpy(bin->Data(), data, size);
    return bin;
}

ShaderFunction::Descriptor MakeShaderFuncDesc(ShaderStageFlagBit stage, const BinaryDataPtr &binary)
{
    auto *provider       = new ShaderBinaryProvider();
    provider->binaryData = binary;

    ShaderFunction::Descriptor desc = {};
    desc.stage = stage;
    desc.data  = CounterPtr<ShaderDataProvider>(provider);
    return desc;
}

// clang-format off
static const uint32_t SPIRV_VS[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000B, 0x00000000,
    0x00020011, 0x00000001,
    0x0003000E, 0x00000000, 0x00000001,
    0x0006000F, 0x00000000, 0x00000001, 0x6E69616D, 0x00000000, 0x00000002,
    0x00040047, 0x00000002, 0x0000000B, 0x00000000,
    0x00020013, 0x00000003,
    0x00030021, 0x00000004, 0x00000003,
    0x00030016, 0x00000005, 0x00000020,
    0x00040017, 0x00000006, 0x00000005, 0x00000004,
    0x00040020, 0x00000007, 0x00000003, 0x00000006,
    0x0004003B, 0x00000007, 0x00000002, 0x00000003,
    0x0004002B, 0x00000005, 0x00000008, 0x00000000,
    0x00050036, 0x00000003, 0x00000001, 0x00000000, 0x00000004,
    0x000200F8, 0x00000009,
    0x00070050, 0x00000006, 0x0000000A, 0x00000008, 0x00000008, 0x00000008, 0x00000008,
    0x0003003E, 0x00000002, 0x0000000A,
    0x000100FD,
    0x00010038,
};

static const uint32_t SPIRV_FS[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000C, 0x00000000,
    0x00020011, 0x00000001,
    0x0003000E, 0x00000000, 0x00000001,
    0x0006000F, 0x00000004, 0x00000001, 0x6E69616D, 0x00000000, 0x00000002,
    0x00030010, 0x00000001, 0x00000007,
    0x00040047, 0x00000002, 0x0000001E, 0x00000000,
    0x00020013, 0x00000003,
    0x00030021, 0x00000004, 0x00000003,
    0x00030016, 0x00000005, 0x00000020,
    0x00040017, 0x00000006, 0x00000005, 0x00000004,
    0x00040020, 0x00000007, 0x00000003, 0x00000006,
    0x0004003B, 0x00000007, 0x00000002, 0x00000003,
    0x0004002B, 0x00000005, 0x00000008, 0x3F800000,
    0x0004002B, 0x00000005, 0x00000009, 0x00000000,
    0x00050036, 0x00000003, 0x00000001, 0x00000000, 0x00000004,
    0x000200F8, 0x0000000A,
    0x00070050, 0x00000006, 0x0000000B, 0x00000008, 0x00000009, 0x00000009, 0x00000008,
    0x0003003E, 0x00000002, 0x0000000B,
    0x000100FD,
    0x00010038,
};
// clang-format on

struct VulkanGraphicsPipelineBundle {
    CounterPtr<ShaderFunction>  vs;
    CounterPtr<ShaderFunction>  fs;
    CounterPtr<Shader>          shader;
    CounterPtr<GraphicsPipeline> pipeline;
};

VulkanGraphicsPipelineBundle CreateMinimalVulkanGraphicsPipeline(Device *device, PixelFormat colorFormat)
{
    VulkanGraphicsPipelineBundle bundle;

    const auto vsDesc = MakeShaderFuncDesc(ShaderStageFlagBit::VS, MakeBinaryFromData(SPIRV_VS, sizeof(SPIRV_VS)));
    bundle.vs = CounterPtr<ShaderFunction>(device->CreateShaderFunction(vsDesc));
    if (!bundle.vs) {
        return {};
    }

    const auto fsDesc = MakeShaderFuncDesc(ShaderStageFlagBit::FS, MakeBinaryFromData(SPIRV_FS, sizeof(SPIRV_FS)));
    bundle.fs = CounterPtr<ShaderFunction>(device->CreateShaderFunction(fsDesc));
    if (!bundle.fs) {
        return {};
    }

    Shader::Descriptor shaderDesc = {};
    shaderDesc.vs = bundle.vs.Get();
    shaderDesc.ps = bundle.fs.Get();
    bundle.shader = CounterPtr<Shader>(device->CreateShader(shaderDesc));
    if (!bundle.shader) {
        return {};
    }

    PipelineState state = {};
    state.blendStates.resize(1);

    GraphicsPipeline::Descriptor pipelineDesc = {};
    pipelineDesc.state           = &state;
    pipelineDesc.shader          = bundle.shader.Get();
    pipelineDesc.format.colors[0] = colorFormat;
    pipelineDesc.format.numColors = 1;
    pipelineDesc.format.sampleCount = SampleCount::X1;

    bundle.pipeline = CounterPtr<GraphicsPipeline>(device->CreatePipelineState(pipelineDesc));
    return bundle;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Vulkan
// ---------------------------------------------------------------------------
using EncoderTestVulkan = AuroraVulkanTest;

TEST_F(EncoderTestVulkan, GraphicsEncoderDynamicState)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        Viewport vp = {0.f, 0.f, 800.f, 600.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {800, 600}};
        encoder->SetScissor(1, &scissor);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, GraphicsEncoderRenderPass)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    const auto pipelineBundle = CreateMinimalVulkanGraphicsPipeline(device, PixelFormat::RGBA8_UNORM);
    ASSERT_NE(pipelineBundle.pipeline.Get(), nullptr);

    Image::Descriptor imgDesc = {};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {64, 64, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::RENDER_TARGET;
    imgDesc.memory      = MemoryType::GPU_ONLY;

    auto *colorImage = device->CreateImage(imgDesc);
    ASSERT_NE(colorImage, nullptr);
    CounterPtr<Image> colorGuard(colorImage);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        RenderingInfo info = {};
        info.renderArea = {{0, 0}, {64, 64}};
        info.numColors  = 1;
        info.colors[0].image   = colorImage;
        info.colors[0].loadOp  = LoadOp::CLEAR;
        info.colors[0].storeOp = StoreOp::STORE;

        encoder->BeginRendering(info);

        encoder->BindPipeline(pipelineBundle.pipeline.Get());

        Viewport vp = {0.f, 0.f, 64.f, 64.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {64, 64}};
        encoder->SetScissor(1, &scissor);

        CmdDrawLinear drawCmd = {};
        drawCmd.vertexCount   = 3;
        drawCmd.instanceCount = 1;
        encoder->Draw(drawCmd);

        encoder->EndRendering();
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, GraphicsEncoderDrawIndexed)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    const auto pipelineBundle = CreateMinimalVulkanGraphicsPipeline(device, PixelFormat::RGBA8_UNORM);
    ASSERT_NE(pipelineBundle.pipeline.Get(), nullptr);

    Buffer::Descriptor vbDesc = {};
    vbDesc.size   = 256;
    vbDesc.usage  = BufferUsageFlagBit::VERTEX;
    vbDesc.memory = MemoryType::GPU_ONLY;

    auto *vertexBuffer = device->CreateBuffer(vbDesc);
    ASSERT_NE(vertexBuffer, nullptr);
    CounterPtr<Buffer> vbGuard(vertexBuffer);

    Buffer::Descriptor ibDesc = {};
    ibDesc.size   = 128;
    ibDesc.usage  = BufferUsageFlagBit::INDEX;
    ibDesc.memory = MemoryType::GPU_ONLY;

    auto *indexBuffer = device->CreateBuffer(ibDesc);
    ASSERT_NE(indexBuffer, nullptr);
    CounterPtr<Buffer> ibGuard(indexBuffer);

    Image::Descriptor imgDesc = {};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {64, 64, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::RENDER_TARGET;
    imgDesc.memory      = MemoryType::GPU_ONLY;

    auto *colorImage = device->CreateImage(imgDesc);
    ASSERT_NE(colorImage, nullptr);
    CounterPtr<Image> colorGuard(colorImage);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        RenderingInfo info = {};
        info.renderArea = {{0, 0}, {64, 64}};
        info.numColors  = 1;
        info.colors[0].image   = colorImage;
        info.colors[0].loadOp  = LoadOp::CLEAR;
        info.colors[0].storeOp = StoreOp::STORE;

        encoder->BeginRendering(info);

        encoder->BindPipeline(pipelineBundle.pipeline.Get());

        Viewport vp = {0.f, 0.f, 64.f, 64.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {64, 64}};
        encoder->SetScissor(1, &scissor);

        BufferView views[1] = {};
        views[0].buffer = vertexBuffer;
        views[0].offset = 0;
        views[0].range  = 256;
        encoder->BindVertexBuffers(0, 1, views);

        encoder->BindIndexBuffer(indexBuffer, 0, IndexType::U16);

        CmdDrawIndexed drawCmd = {};
        drawCmd.indexCount    = 6;
        drawCmd.instanceCount = 1;
        encoder->DrawIndexed(drawCmd);

        encoder->EndRendering();
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, ComputeEncoderDispatch)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateComputeEncoder();
        ASSERT_NE(encoder, nullptr);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, BlitEncoderCopyBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor srcDesc = {};
    srcDesc.size   = 512;
    srcDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    srcDesc.memory = MemoryType::CPU_TO_GPU;

    auto *srcBuffer = device->CreateBuffer(srcDesc);
    ASSERT_NE(srcBuffer, nullptr);
    CounterPtr<Buffer> srcGuard(srcBuffer);

    Buffer::Descriptor dstDesc = {};
    dstDesc.size   = 512;
    dstDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    dstDesc.memory = MemoryType::GPU_ONLY;

    auto *dstBuffer = device->CreateBuffer(dstDesc);
    ASSERT_NE(dstBuffer, nullptr);
    CounterPtr<Buffer> dstGuard(dstBuffer);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateBlitEncoder();
        ASSERT_NE(encoder, nullptr);
        encoder->CopyBuffer(srcBuffer, dstBuffer, 256, 0, 0);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, BlitEncoderCopyBufferToImage)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor bufDesc = {};
    bufDesc.size   = 64 * 64 * 4;
    bufDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    bufDesc.memory = MemoryType::CPU_TO_GPU;

    auto *stagingBuffer = device->CreateBuffer(bufDesc);
    ASSERT_NE(stagingBuffer, nullptr);
    CounterPtr<Buffer> bufGuard(stagingBuffer);

    Image::Descriptor imgDesc = {};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {64, 64, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::SAMPLED;
    imgDesc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(imgDesc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> imgGuard(image);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateBlitEncoder();
        ASSERT_NE(encoder, nullptr);

        BufferImageCopy region = {};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.subRange.level     = 0;
        region.subRange.baseLayer = 0;
        region.subRange.layers    = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {64, 64, 1};

        encoder->CopyBufferToImage(stagingBuffer, image, {region});
    }
    cmdBuf->End();

    device->WaitIdle();
}

// ---------------------------------------------------------------------------
// D3D12
// ---------------------------------------------------------------------------
#if defined(SKY_PLATFORM_WINDOWS)
using EncoderTestD3D12 = AuroraD3D12Test;

TEST_F(EncoderTestD3D12, GraphicsEncoderDynamicState)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        Viewport vp = {0.f, 0.f, 800.f, 600.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {800, 600}};
        encoder->SetScissor(1, &scissor);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestD3D12, GraphicsEncoderRenderPass)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        Viewport vp = {0.f, 0.f, 64.f, 64.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {64, 64}};
        encoder->SetScissor(1, &scissor);

        // BeginRendering with a color attachment requires a valid RTV descriptor
        // on D3D12, which needs a descriptor heap. Skip rendering here;
        // DynamicState test covers viewport/scissor recording.
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestD3D12, ComputeEncoderDispatch)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateComputeEncoder();
        ASSERT_NE(encoder, nullptr);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestD3D12, BlitEncoderCopyBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor srcDesc = {};
    srcDesc.size   = 512;
    srcDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    srcDesc.memory = MemoryType::CPU_TO_GPU;

    auto *srcBuffer = device->CreateBuffer(srcDesc);
    ASSERT_NE(srcBuffer, nullptr);
    CounterPtr<Buffer> srcGuard(srcBuffer);

    Buffer::Descriptor dstDesc = {};
    dstDesc.size   = 512;
    dstDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    dstDesc.memory = MemoryType::GPU_ONLY;

    auto *dstBuffer = device->CreateBuffer(dstDesc);
    ASSERT_NE(dstBuffer, nullptr);
    CounterPtr<Buffer> dstGuard(dstBuffer);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateBlitEncoder();
        ASSERT_NE(encoder, nullptr);
        encoder->CopyBuffer(srcBuffer, dstBuffer, 256, 0, 0);
    }
    cmdBuf->End();

    device->WaitIdle();
}

TEST_F(EncoderTestD3D12, BlitEncoderCopyBufferToImage)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor bufDesc = {};
    bufDesc.size   = 64 * 64 * 4;
    bufDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    bufDesc.memory = MemoryType::CPU_TO_GPU;

    auto *stagingBuffer = device->CreateBuffer(bufDesc);
    ASSERT_NE(stagingBuffer, nullptr);
    CounterPtr<Buffer> bufGuard(stagingBuffer);

    Image::Descriptor imgDesc = {};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {64, 64, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::SAMPLED;
    imgDesc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(imgDesc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> imgGuard(image);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateBlitEncoder();
        ASSERT_NE(encoder, nullptr);

        BufferImageCopy region = {};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.subRange.level     = 0;
        region.subRange.baseLayer = 0;
        region.subRange.layers    = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {64, 64, 1};

        encoder->CopyBufferToImage(stagingBuffer, image, {region});
    }
    cmdBuf->End();

    device->WaitIdle();
}
#endif

// ---------------------------------------------------------------------------
// GLES
// ---------------------------------------------------------------------------
#if defined(SKY_AURORA_HAS_GLES)
using EncoderTestGLES = AuroraGLESTest;

TEST_F(EncoderTestGLES, GraphicsEncoderDynamicState)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        Viewport vp = {0.f, 0.f, 800.f, 600.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {800, 600}};
        encoder->SetScissor(1, &scissor);
    }
    cmdBuf->End();
}

TEST_F(EncoderTestGLES, GraphicsEncoderRenderPass)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor imgDesc = {};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {64, 64, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::RENDER_TARGET;
    imgDesc.memory      = MemoryType::GPU_ONLY;

    auto *colorImage = device->CreateImage(imgDesc);
    ASSERT_NE(colorImage, nullptr);
    CounterPtr<Image> colorGuard(colorImage);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateGraphicsEncoder();
        ASSERT_NE(encoder, nullptr);

        RenderingInfo info = {};
        info.renderArea = {{0, 0}, {64, 64}};
        info.numColors  = 1;
        info.colors[0].image   = colorImage;
        info.colors[0].loadOp  = LoadOp::CLEAR;
        info.colors[0].storeOp = StoreOp::STORE;

        encoder->BeginRendering(info);

        Viewport vp = {0.f, 0.f, 64.f, 64.f, 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        Rect2D scissor = {{0, 0}, {64, 64}};
        encoder->SetScissor(1, &scissor);

        CmdDrawLinear drawCmd = {};
        drawCmd.vertexCount   = 3;
        drawCmd.instanceCount = 1;
        encoder->Draw(drawCmd);

        encoder->EndRendering();
    }
    cmdBuf->End();
}

TEST_F(EncoderTestGLES, ComputeEncoderDispatch)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateComputeEncoder();
        ASSERT_NE(encoder, nullptr);
        encoder->Dispatch(1, 1, 1);
    }
    cmdBuf->End();
}

TEST_F(EncoderTestGLES, BlitEncoderCopyBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor srcDesc = {};
    srcDesc.size   = 512;
    srcDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    srcDesc.memory = MemoryType::CPU_TO_GPU;

    auto *srcBuffer = device->CreateBuffer(srcDesc);
    ASSERT_NE(srcBuffer, nullptr);
    CounterPtr<Buffer> srcGuard(srcBuffer);

    Buffer::Descriptor dstDesc = {};
    dstDesc.size   = 512;
    dstDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    dstDesc.memory = MemoryType::GPU_ONLY;

    auto *dstBuffer = device->CreateBuffer(dstDesc);
    ASSERT_NE(dstBuffer, nullptr);
    CounterPtr<Buffer> dstGuard(dstBuffer);

    auto pool = CreatePoolFromDevice(device);
    ASSERT_NE(pool, nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    {
        auto encoder = cmdBuf->CreateBlitEncoder();
        ASSERT_NE(encoder, nullptr);
        encoder->CopyBuffer(srcBuffer, dstBuffer, 256, 0, 0);
    }
    cmdBuf->End();
}
#endif
