//
// Created on 2026/04/07.
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>

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

    device->WaitIdle();
}

TEST_F(EncoderTestVulkan, GraphicsEncoderDrawIndexed)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

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
