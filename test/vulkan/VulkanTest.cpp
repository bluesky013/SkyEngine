//
// Created by Zach Lee on 2022/1/10.
//

#include <gtest/gtest.h>
#include <vulkan/Driver.h>
#include <vulkan/Device.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Sampler.h>
#include <vulkan/ShaderOption.h>

using namespace sky::drv;



TEST(VulkanTest, PipelineLayoutTest)
{
    Driver::Descriptor drvDes = {};
    drvDes.engineName = "SkyEngine";
    drvDes.appName = "Test";
    drvDes.enableDebugLayer = true;

    auto driver = Driver::Create(drvDes);

    Device::Descriptor devDes = {};
    auto device = driver->CreateDevice(devDes);

    PipelineLayout::Descriptor pipelineLayoutDes = {};

    {
        DescriptorSetLayout::Descriptor descriptor = {};
        descriptor.bindings.emplace(0, DescriptorSetLayout::SetBinding
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, 0});
        descriptor.bindings.emplace(1, DescriptorSetLayout::SetBinding
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0});
        descriptor.bindings.emplace(2, DescriptorSetLayout::SetBinding
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0});

        auto setLayout = device->CreateDeviceObject<DescriptorSetLayout>(descriptor);
        ASSERT_NE(setLayout, nullptr);
        ASSERT_NE(setLayout->GetNativeHandle(), VK_NULL_HANDLE);
        pipelineLayoutDes.desLayouts.emplace(0, setLayout->GetHash());
    }

    {
        DescriptorSetLayout::Descriptor descriptor = {};
        descriptor.bindings.emplace(0, DescriptorSetLayout::SetBinding
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, 0});
        descriptor.bindings.emplace(1, DescriptorSetLayout::SetBinding
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0});

        auto setLayout = device->CreateDeviceObject<DescriptorSetLayout>(descriptor);
        ASSERT_NE(setLayout, nullptr);
        ASSERT_NE(setLayout->GetNativeHandle(), VK_NULL_HANDLE);
        pipelineLayoutDes.desLayouts.emplace(1, setLayout->GetHash());
    }

    auto pipelineLayout = device->CreateDeviceObject<PipelineLayout>(pipelineLayoutDes);
    ASSERT_NE(pipelineLayout, nullptr);
    ASSERT_NE(pipelineLayout->GetNativeHandle(), VK_NULL_HANDLE);
    ASSERT_EQ(device->GetPipelineLayout(pipelineLayout->GetHash()), pipelineLayout->GetNativeHandle());

    auto& requirements = pipelineLayout->GetRequirements();
    ASSERT_EQ(requirements.size(), 2);
    ASSERT_EQ(requirements[0], 0);
    ASSERT_EQ(requirements[1], 1);
}

TEST(VulkanTest, ShaderOptionTest)
{
    ShaderOption::Builder builder;

    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4);
    builder.AddConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, 4);

    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1, 4);
    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 2, 4);
    auto ptr = builder.Build();

    ASSERT_EQ(!!ptr, true);
}