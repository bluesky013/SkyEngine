//
// Aurora ResourceGroup tests (Vulkan + DX12).
// ResourceGroup is created from {shader, set}; its layout is derived from the
// shader reflection, not from a hand-written ResourceGroupLayout.
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/ShaderReflection.h>
#include <core/archive/BinaryData.h>
#include <cstring>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    // Minimal compute SPIR-V: layout(local_size_x=1) in; void main() {}
    static const uint32_t SPIRV_CS[] = {
        0x07230203, 0x00010000, 0x00000000, 0x00000005, 0x00000000,
        0x00020011, 0x00000001,
        0x0003000E, 0x00000000, 0x00000001,
        0x0005000F, 0x00000005, 0x00000001, 0x6E69616D, 0x00000000,
        0x00060010, 0x00000001, 0x00000011, 0x00000001, 0x00000001, 0x00000001,
        0x00020013, 0x00000002,
        0x00030021, 0x00000003, 0x00000002,
        0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        0x000200F8, 0x00000004,
        0x000100FD,
        0x00010038,
    };

    Shader *MakeComputeShader(Device *device, const ShaderReflection &reflection)
    {
        auto binary = CounterPtr<BinaryData>(new BinaryData(sizeof(SPIRV_CS)));
        std::memcpy(binary->Data(), SPIRV_CS, sizeof(SPIRV_CS));

        auto *provider       = new ShaderBinaryProvider();
        provider->binaryData = binary;

        ShaderFunction::Descriptor fnDesc = {};
        fnDesc.stage = ShaderStageFlagBit::CS;
        fnDesc.data  = CounterPtr<ShaderDataProvider>(provider);
        auto *cs = device->CreateShaderFunction(fnDesc);
        if (cs == nullptr) {
            return nullptr;
        }

        Shader::Descriptor shaderDesc = {};
        shaderDesc.cs         = cs;
        shaderDesc.reflection = &reflection;
        return device->CreateShader(shaderDesc);
    }

    ShaderReflection MakeUboReflection(uint32_t set)
    {
        ShaderReflection refl{};
        ShaderResource   res{};
        res.name    = "Ubo";
        res.set     = set;
        res.binding = 0;
        res.type    = ShaderResourceType::UNIFORM_BUFFER;
        res.count   = 1;
        refl.resources.push_back(res);
        return refl;
    }

    ShaderReflection MakeDynamicUboReflection(uint32_t set)
    {
        ShaderReflection refl{};
        ShaderResource   res{};
        res.name    = "BatchUbo";
        res.set     = set;
        res.binding = 0;
        res.type    = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        res.count   = 1;
        refl.resources.push_back(res);
        return refl;
    }

    ShaderReflection MakeMixedReflection(uint32_t set)
    {
        ShaderReflection refl{};

        ShaderResource ubo{};
        ubo.name = "Ubo";
        ubo.set = set;
        ubo.binding = 0;
        ubo.type = ShaderResourceType::UNIFORM_BUFFER;
        ubo.count = 1;
        refl.resources.push_back(ubo);

        ShaderResource img{};
        img.name = "Tex";
        img.set = set;
        img.binding = 1;
        img.type = ShaderResourceType::SAMPLED_IMAGE;
        img.count = 1;
        refl.resources.push_back(img);

        ShaderResource smp{};
        smp.name = "Smp";
        smp.set = set;
        smp.binding = 2;
        smp.type = ShaderResourceType::SAMPLER;
        smp.count = 1;
        refl.resources.push_back(smp);

        return refl;
    }

} // namespace

using ResourceGroupTestVulkan = AuroraVulkanTest;

TEST_F(ResourceGroupTestVulkan, CreateEmptyGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ShaderReflection refl{}; // empty reflection: shader creation is valid
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, refl));
    ASSERT_NE(shader.Get(), nullptr);

    // set 0 does not exist in an empty reflection
    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 0;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestVulkan, GroupRequiresShader)
{
    auto *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.shader = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestVulkan, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    encoder->WriteBuffer(0, ub.Get(), 0, 256);
    encoder->End(); // should not assert / crash
    SUCCEED();
}

TEST_F(ResourceGroupTestVulkan, EncoderBatchWrite)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeMixedReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    Image::Descriptor id{};
    id.imageType   = ImageType::IMAGE_2D;
    id.format      = PixelFormat::RGBA8_UNORM;
    id.extent      = {16, 16, 1};
    id.mipLevels   = 1;
    id.arrayLayers = 1;
    id.samples     = SampleCount::X1;
    id.usage       = ImageUsageFlagBit::SAMPLED;
    id.memory      = MemoryType::GPU_ONLY;
    auto img = CounterPtr<Image>(device->CreateImage(id));
    ASSERT_NE(img.Get(), nullptr);

    Sampler::Descriptor sd{};
    sd.magFilter    = Filter::LINEAR;
    sd.minFilter    = Filter::LINEAR;
    sd.mipmapMode   = MipFilter::LINEAR;
    sd.addressModeU = WrapMode::REPEAT;
    sd.addressModeV = WrapMode::REPEAT;
    sd.addressModeW = WrapMode::REPEAT;
    auto smp = CounterPtr<Sampler>(device->CreateSampler(sd));
    ASSERT_NE(smp.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    encoder->WriteBuffer(0, ub.Get(), 0, 256);
    encoder->WriteImage(1, img.Get(), ImageLayout::SHADER_READ_ONLY);
    encoder->WriteSampler(2, smp.Get());
    encoder->End(); // batched write must not assert / crash
    SUCCEED();
}

TEST_F(ResourceGroupTestVulkan, DescriptorBatchCrossSet)
{
    auto *device = GetDevice();

    auto shader0 = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    auto shader1 = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(1)));
    ASSERT_NE(shader0.Get(), nullptr);
    ASSERT_NE(shader1.Get(), nullptr);

    ResourceGroup::Descriptor gd0{};
    gd0.shader = shader0.Get();
    gd0.set    = 0;
    ResourceGroup::Descriptor gd1{};
    gd1.shader = shader1.Get();
    gd1.set    = 1;
    auto group0 = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd0));
    auto group1 = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd1));
    ASSERT_NE(group0.Get(), nullptr);
    ASSERT_NE(group1.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto *batch = device->CreateDescriptorBatch();
    ASSERT_NE(batch, nullptr);

    batch->WriteBuffer(group0.Get(), 0, ub.Get(), 0, 256);
    batch->WriteBuffer(group1.Get(), 0, ub.Get(), 0, 256);
    batch->Flush(); // single vkUpdateDescriptorSets across two sets
    batch->Reset();

    delete batch;
    SUCCEED();
}

TEST_F(ResourceGroupTestVulkan, SetIndexHole)
{
    auto *device = GetDevice();

    // only set 2 has a resource; set 0/1 are absent (hole)
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

#if defined(SKY_PLATFORM_WINDOWS)
using ResourceGroupTestD3D12 = AuroraD3D12Test;

TEST_F(ResourceGroupTestD3D12, CreateEmptyGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ShaderReflection refl{};
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, refl));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 0;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestD3D12, GroupRequiresShader)
{
    auto *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.shader = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestD3D12, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    encoder->WriteBuffer(0, ub.Get(), 0, 256);
    encoder->End();
    SUCCEED();
}

TEST_F(ResourceGroupTestD3D12, SetIndexHole)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestD3D12, UpdateDynamicUniformBuffer)
{
    auto *device = GetDevice();

    // root CBV is derived from the reflection; verify it serializes + updates
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeDynamicUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader = shader.Get();
    gd.set    = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 4096;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    encoder->WriteBuffer(0, ub.Get(), 0, 256); // dynamic binding: recorded, no descriptor write
    encoder->End();
    SUCCEED();
}
#endif
