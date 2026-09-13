//
// Aurora ResourceGroup tests (Vulkan).
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/ResourceGroup.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

using ResourceGroupTestVulkan = AuroraVulkanTest;

TEST_F(ResourceGroupTestVulkan, CreateEmptyLayoutAndGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ResourceGroupLayout::Descriptor desc{};
    auto layout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(desc));
    ASSERT_NE(layout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = layout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestVulkan, LayoutWithUniformAndSampledImage)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ResourceGroupLayout::Descriptor desc{};
    desc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::GFX, {}});
    desc.bindings.push_back({1, DescriptorType::COMBINED_IMAGE_SAMPLER, 1, ShaderStageFlagBit::FS, {}});

    auto layout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(desc));
    ASSERT_NE(layout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = layout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestVulkan, GroupRequiresLayout)
{
    auto *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.layout = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestVulkan, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    ResourceGroupLayout::Descriptor rgDesc{};
    rgDesc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::GFX, {}});
    auto rgLayout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(rgDesc));
    ASSERT_NE(rgLayout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = rgLayout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    ResourceUpdateInfo w{};
    w.binding      = 0;
    w.kind         = ResourceWriteKind::BUFFER;
    w.buffer       = ub.Get();
    w.bufferOffset = 0;
    w.bufferRange  = 256;

    group->Update({w});       // should not assert / crash
    SUCCEED();
}

#if defined(SKY_PLATFORM_WINDOWS)
using ResourceGroupTestD3D12 = AuroraD3D12Test;

TEST_F(ResourceGroupTestD3D12, CreateEmptyLayoutAndGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ResourceGroupLayout::Descriptor desc{};
    auto layout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(desc));
    ASSERT_NE(layout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = layout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestD3D12, LayoutWithUniformAndSampledImage)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ResourceGroupLayout::Descriptor desc{};
    desc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::GFX, {}});
    desc.bindings.push_back({1, DescriptorType::COMBINED_IMAGE_SAMPLER, 1, ShaderStageFlagBit::FS, {}});

    auto layout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(desc));
    ASSERT_NE(layout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = layout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestD3D12, GroupRequiresLayout)
{
    auto *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.layout = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestD3D12, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    ResourceGroupLayout::Descriptor rgDesc{};
    rgDesc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::GFX, {}});
    auto rgLayout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(rgDesc));
    ASSERT_NE(rgLayout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = rgLayout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    ResourceUpdateInfo w{};
    w.binding      = 0;
    w.kind         = ResourceWriteKind::BUFFER;
    w.buffer       = ub.Get();
    w.bufferOffset = 0;
    w.bufferRange  = 256;

    group->Update({w});       // should not assert / crash
    SUCCEED();
}

TEST_F(ResourceGroupTestD3D12, UpdateCombinedImageSampler)
{
    auto *device = GetDevice();

    ResourceGroupLayout::Descriptor rgDesc{};
    rgDesc.bindings.push_back({0, DescriptorType::COMBINED_IMAGE_SAMPLER, 1, ShaderStageFlagBit::FS, {}});
    auto rgLayout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(rgDesc));
    ASSERT_NE(rgLayout.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.layout = rgLayout.Get();
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Image::Descriptor id{};
    id.format      = PixelFormat::RGBA8_UNORM;
    id.extent      = {64, 64, 1};
    id.mipLevels   = 1;
    id.arrayLayers = 1;
    id.usage       = ImageUsageFlagBit::SAMPLED;
    id.memory      = MemoryType::GPU_ONLY;
    auto img = CounterPtr<Image>(device->CreateImage(id));
    ASSERT_NE(img.Get(), nullptr);

    Sampler::Descriptor sd{};
    auto smp = CounterPtr<Sampler>(device->CreateSampler(sd));
    ASSERT_NE(smp.Get(), nullptr);

    ResourceUpdateInfo w{};
    w.binding = 0;
    w.kind    = ResourceWriteKind::COMBINED_IMAGE_SAMPLER;
    w.image   = img.Get();
    w.sampler = smp.Get();

    group->Update({w});       // should split into SRV + sampler and not crash
    SUCCEED();
}
#endif
