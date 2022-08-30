//
// Created by Zach Lee on 2022/1/10.
//

#include <gtest/gtest.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/Device.h>
#include <vulkan/Driver.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Sampler.h>
#include <vulkan/ShaderOption.h>
#include <vulkan/VertexInput.h>

using namespace sky::drv;

TEST(VulkanTest, PipelineLayoutTest)
{
    Driver::Descriptor drvDes = {};
    drvDes.engineName         = "SkyEngine";
    drvDes.appName            = "Test";
    drvDes.enableDebugLayer   = true;

    auto driver = Driver::Create(drvDes);

    Device::Descriptor devDes = {};
    auto               device = driver->CreateDevice(devDes);

    PipelineLayout::Descriptor pipelineLayoutDes = {};

    {
        DescriptorSetLayout::Descriptor descriptor = {};
        descriptor.bindings.emplace(0, DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});
        descriptor.bindings.emplace(1, DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        descriptor.bindings.emplace(2, DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        pipelineLayoutDes.desLayouts.emplace_back(descriptor);
    }

    {
        DescriptorSetLayout::Descriptor descriptor = {};
        descriptor.bindings.emplace(0, DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});
        descriptor.bindings.emplace(1, DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        pipelineLayoutDes.desLayouts.emplace_back(descriptor);
    }

    auto pipelineLayout = device->CreateDeviceObject<PipelineLayout>(pipelineLayoutDes);
    ASSERT_NE(pipelineLayout, nullptr);
    ASSERT_NE(pipelineLayout->GetNativeHandle(), nullptr);
    ASSERT_EQ(device->GetPipelineLayout(pipelineLayout->GetHash()), pipelineLayout->GetNativeHandle());
}

TEST(VulkanTest, ShaderOptionTest)
{
    ShaderOption::Builder builder;

    struct OpDataTest {
        uint32_t a = 0;
        float    b = 0;
        uint32_t c = 0;
        float    d = 0;
    };

    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4);
    builder.AddConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, 4);

    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1, 4);
    builder.AddConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 2, 4);
    auto ptr = builder.Build();

    ASSERT_EQ(!!ptr, true);
    ptr->SetConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, 1);
    ptr->SetConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 0.2f);
    ptr->SetConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1, 3);
    ptr->SetConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 2, 0.4f);

    auto data = ptr->GetData();

    auto vi = ptr->GetSpecializationInfo(VK_SHADER_STAGE_VERTEX_BIT);
    ASSERT_NE(vi, nullptr);
    ASSERT_EQ(vi->mapEntryCount, 1);
    ASSERT_EQ(vi->dataSize, 4);
    ASSERT_EQ(data, vi->pData);

    auto fi = ptr->GetSpecializationInfo(VK_SHADER_STAGE_FRAGMENT_BIT);
    ASSERT_NE(fi, nullptr);
    ASSERT_EQ(fi->mapEntryCount, 3);
    ASSERT_EQ(fi->dataSize, 12);
    ASSERT_EQ(&((uint8_t *)vi->pData)[4], fi->pData);

    auto tData = (const OpDataTest *)data;
    ASSERT_EQ(tData->a, 1);
    ASSERT_EQ(tData->b, 0.2f);
    ASSERT_EQ(tData->c, 3);
    ASSERT_EQ(tData->d, 0.4f);
}

TEST(VulkanTest, VertexInputTest)
{
    VertexInput::Builder builder;
    auto                 ptr = builder.Begin()
                   .AddAttribute(0, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT)
                   .AddAttribute(1, 0, 16, VK_FORMAT_R32G32B32A32_SFLOAT)
                   .AddAttribute(2, 0, 32, VK_FORMAT_R32G32B32A32_SFLOAT)
                   .AddAttribute(3, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT)
                   .AddStream(0, 48, VK_VERTEX_INPUT_RATE_VERTEX)
                   .AddStream(1, 4, VK_VERTEX_INPUT_RATE_VERTEX)
                   .Build();
    ASSERT_NE(ptr.get(), nullptr);
}