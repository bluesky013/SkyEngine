//
// Created on 2026/04/01.
//

#include "AuroraTestHelper.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
using ResourceTestMetal = AuroraMetalTest;
#endif
#if defined(SKY_PLATFORM_WINDOWS)
using ResourceTestD3D12 = AuroraD3D12Test;
#endif
#if defined(SKY_AURORA_HAS_GLES)
using ResourceTestGLES = AuroraGLESTest;
#endif

// ---------------------------------------------------------------------------
// Vulkan
// ---------------------------------------------------------------------------
using ResourceTestVulkan = AuroraVulkanTest;

TEST_F(ResourceTestVulkan, CreateBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 1024;
    desc.usage  = BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestVulkan, CreateBufferZeroSize)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 0;
    desc.usage  = BufferUsageFlagBit::VERTEX;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(ResourceTestVulkan, CreateBufferCpuVisible)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 256;
    desc.usage  = BufferUsageFlagBit::UNIFORM;
    desc.memory = MemoryType::CPU_TO_GPU;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestVulkan, CreateImage2D)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {256, 256, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestVulkan, CreateImageDepthStencil)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType = ImageType::IMAGE_2D;
    desc.format    = PixelFormat::D24_S8;
    desc.extent    = {512, 512, 1};
    desc.usage     = ImageUsageFlagBit::DEPTH_STENCIL;
    desc.memory    = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestVulkan, CreateImageMipmapped)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType = ImageType::IMAGE_2D;
    desc.format    = PixelFormat::RGBA8_SRGB;
    desc.extent    = {512, 512, 1};
    desc.mipLevels = 10;
    desc.usage     = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory    = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestVulkan, CreateSampler)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Sampler::Descriptor desc = {};
    desc.magFilter    = Filter::LINEAR;
    desc.minFilter    = Filter::LINEAR;
    desc.mipmapMode   = MipFilter::LINEAR;
    desc.addressModeU = WrapMode::REPEAT;
    desc.addressModeV = WrapMode::REPEAT;
    desc.addressModeW = WrapMode::REPEAT;
    desc.minLod       = 0.f;
    desc.maxLod       = 12.f;
    desc.maxAnisotropy = 16.f;
    desc.anisotropyEnable = true;

    auto *sampler = device->CreateSampler(desc);
    ASSERT_NE(sampler, nullptr);
    CounterPtr<Sampler> guard(sampler);
}

TEST_F(ResourceTestVulkan, CreateSamplerNearest)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Sampler::Descriptor desc = {};
    desc.magFilter    = Filter::NEAREST;
    desc.minFilter    = Filter::NEAREST;
    desc.mipmapMode   = MipFilter::NEAREST;
    desc.addressModeU = WrapMode::CLAMP_TO_EDGE;
    desc.addressModeV = WrapMode::CLAMP_TO_EDGE;
    desc.addressModeW = WrapMode::CLAMP_TO_EDGE;

    auto *sampler = device->CreateSampler(desc);
    ASSERT_NE(sampler, nullptr);
    CounterPtr<Sampler> guard(sampler);
}

// ---------------------------------------------------------------------------
// D3D12
// ---------------------------------------------------------------------------
#if defined(SKY_PLATFORM_WINDOWS)
TEST_F(ResourceTestD3D12, CreateBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 1024;
    desc.usage  = BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestD3D12, CreateBufferZeroSize)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 0;
    desc.usage  = BufferUsageFlagBit::VERTEX;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(ResourceTestD3D12, CreateImage2D)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {256, 256, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestD3D12, CreateSampler)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Sampler::Descriptor desc = {};
    desc.magFilter         = Filter::LINEAR;
    desc.minFilter         = Filter::LINEAR;
    desc.mipmapMode        = MipFilter::LINEAR;
    desc.addressModeU      = WrapMode::REPEAT;
    desc.addressModeV      = WrapMode::REPEAT;
    desc.addressModeW      = WrapMode::REPEAT;
    desc.maxAnisotropy     = 16.f;
    desc.anisotropyEnable  = true;

    auto *sampler = device->CreateSampler(desc);
    ASSERT_NE(sampler, nullptr);
    CounterPtr<Sampler> guard(sampler);
}
#endif

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
TEST_F(ResourceTestMetal, CreateBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 1024;
    desc.usage  = BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestMetal, CreateBufferZeroSize)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 0;
    desc.usage  = BufferUsageFlagBit::VERTEX;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(ResourceTestMetal, CreateImage2D)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {256, 256, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestMetal, CreateImageDepthStencil)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType = ImageType::IMAGE_2D;
    desc.format    = PixelFormat::D24_S8;
    desc.extent    = {512, 512, 1};
    desc.usage     = ImageUsageFlagBit::DEPTH_STENCIL;
    desc.memory    = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestMetal, CreateSampler)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Sampler::Descriptor desc = {};
    desc.magFilter        = Filter::LINEAR;
    desc.minFilter        = Filter::LINEAR;
    desc.mipmapMode       = MipFilter::LINEAR;
    desc.addressModeU     = WrapMode::REPEAT;
    desc.addressModeV     = WrapMode::REPEAT;
    desc.addressModeW     = WrapMode::REPEAT;
    desc.maxAnisotropy    = 16.f;
    desc.anisotropyEnable = true;

    auto *sampler = device->CreateSampler(desc);
    ASSERT_NE(sampler, nullptr);
    CounterPtr<Sampler> guard(sampler);
}
#endif

// ---------------------------------------------------------------------------
// GLES
// ---------------------------------------------------------------------------
#if defined(SKY_AURORA_HAS_GLES)
TEST_F(ResourceTestGLES, CreateBuffer)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 1024;
    desc.usage  = BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::GPU_ONLY;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestGLES, CreateBufferVertex)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 4096;
    desc.usage  = BufferUsageFlagBit::VERTEX;
    desc.memory = MemoryType::CPU_TO_GPU;

    auto *buffer = device->CreateBuffer(desc);
    ASSERT_NE(buffer, nullptr);
    CounterPtr<Buffer> guard(buffer);
}

TEST_F(ResourceTestGLES, CreateImage2D)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {128, 128, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;

    auto *image = device->CreateImage(desc);
    ASSERT_NE(image, nullptr);
    CounterPtr<Image> guard(image);
}

TEST_F(ResourceTestGLES, CreateSampler)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Sampler::Descriptor desc = {};
    desc.magFilter    = Filter::LINEAR;
    desc.minFilter    = Filter::LINEAR;
    desc.mipmapMode   = MipFilter::LINEAR;
    desc.addressModeU = WrapMode::REPEAT;
    desc.addressModeV = WrapMode::REPEAT;
    desc.addressModeW = WrapMode::REPEAT;

    auto *sampler = device->CreateSampler(desc);
    ASSERT_NE(sampler, nullptr);
    CounterPtr<Sampler> guard(sampler);
}
#endif
