//
// Created by Zach Lee on 2022/1/10.
//

#include <gtest/gtest.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/Device.h>
#include <vulkan/Instance.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Sampler.h>
#include <vulkan/ShaderOption.h>
#include <vulkan/VertexInput.h>

using namespace sky::vk;

static Instance *instance = nullptr;
static Device *device = nullptr;

class VulkanTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        Instance::Descriptor drvDes = {};
        drvDes.engineName         = "SkyEngine";
        drvDes.appName            = "Test";
        drvDes.enableDebugLayer   = true;

        instance = Instance::Create(drvDes);

        Device::Descriptor devDes = {};
        device = instance->CreateDevice(devDes);
    }

    static void TearDownTestSuite()
    {
        if (device != nullptr) {
            delete device;
            device = nullptr;
        }
        Instance::Destroy(instance);
    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }

};

