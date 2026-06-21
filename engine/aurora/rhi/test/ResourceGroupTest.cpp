//
// Aurora ResourceGroup tests (Vulkan).
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/PipelineLayout.h>
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

TEST_F(ResourceGroupTestVulkan, PipelineLayoutEmpty)
{
    auto *device = GetDevice();

    PipelineLayout::Descriptor desc{};
    auto layout = CounterPtr<PipelineLayout>(device->CreatePipelineLayout(desc));
    ASSERT_NE(layout.Get(), nullptr);
}

TEST_F(ResourceGroupTestVulkan, PipelineLayoutWithGroups)
{
    auto *device = GetDevice();

    ResourceGroupLayout::Descriptor rgDesc{};
    rgDesc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::VS, {}});
    auto rgLayout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(rgDesc));
    ASSERT_NE(rgLayout.Get(), nullptr);

    PipelineLayout::Descriptor plDesc{};
    plDesc.groups.push_back(rgLayout.Get());
    PushConstantRange pc{};
    pc.stageFlags = ShaderStageFlagBit::GFX;
    pc.offset     = 0;
    pc.size       = 16;
    plDesc.pushConstants.push_back(pc);

    auto layout = CounterPtr<PipelineLayout>(device->CreatePipelineLayout(plDesc));
    ASSERT_NE(layout.Get(), nullptr);
}

TEST_F(ResourceGroupTestVulkan, PipelineLayoutTooManyGroupsRejected)
{
    auto *device = GetDevice();

    ResourceGroupLayout::Descriptor rgDesc{};
    rgDesc.bindings.push_back({0, DescriptorType::UNIFORM_BUFFER, 1, ShaderStageFlagBit::VS, {}});
    auto rgLayout = CounterPtr<ResourceGroupLayout>(device->CreateResourceGroupLayout(rgDesc));

    PipelineLayout::Descriptor desc{};
    for (uint32_t i = 0; i < MAX_RESOURCE_GROUPS + 1; ++i) {
        desc.groups.push_back(rgLayout.Get());
    }
    auto *layout = device->CreatePipelineLayout(desc);
    EXPECT_EQ(layout, nullptr);
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
