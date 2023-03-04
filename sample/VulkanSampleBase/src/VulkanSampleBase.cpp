//
// Created by Zach Lee on 2022/10/13.
//

#include "VulkanSampleBase.h"
#include <core/file/FileIO.h>

namespace sky {

    void VulkanSampleBase::OnStart()
    {
        vk::Instance::Descriptor drvDes = {};
        drvDes.enableDebugLayer         = true;
        drvDes.appName                  = "Triangle";
        instance                        = vk::Instance::Create(drvDes);
        if (instance == nullptr) {
            return;
        }

        InitFeature();
        device = instance->CreateDevice(deviceInfo);

        graphicsQueue = device->GetGraphicsQueue();

        auto                       nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        vk::SwapChain::VkDescriptor swcDesc      = {};
        swcDesc.window                          = nativeWindow->GetNativeHandle();
        swapChain                               = device->CreateDeviceObject<vk::SwapChain>(swcDesc);
        Event<IWindowEvent>::Connect(swcDesc.window, this);

        renderPass = vk::RenderPassFactory()()
                         .AddSubPass()
                         .AddColor()
                         .Format(swapChain->GetVkFormat())
                         .Layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                         .ColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                         .Samples(VK_SAMPLE_COUNT_1_BIT)
                         .AddDependency()
                         .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
                         .SetBarrier({VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT})
                         .AddDependency()
                         .SetLinkage(0, VK_SUBPASS_EXTERNAL)
                         .SetBarrier({VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT})
                         .Create(*device);

        ResetFrameBuffer();

        vk::CommandBuffer::VkDescriptor cmdDesc = {};
        commandBuffer = graphicsQueue->AllocateCommandBuffer(cmdDesc);
    }

    void VulkanSampleBase::OnStop()
    {
        swapChain = nullptr;
        imageAvailable = nullptr;
        renderFinish = nullptr;
        renderPass = nullptr;
        commandBuffer = nullptr;
        frameBuffers.clear();
        colorViews.clear();

        delete device;
        vk::Instance::Destroy(instance);
        device = nullptr;
        instance = nullptr;

        Event<IWindowEvent>::DisConnect(this);
    }

    void VulkanSampleBase::OnTick(float delta)
    {
        frameIndex = frame % 2;
        ++frame;
    }

    bool VulkanSampleBase::CheckFeature() const
    {
        auto &features = device->GetFeatures();
        return features.sparseBinding == deviceInfo.feature.sparseBinding &&
            features.descriptorIndexing == deviceInfo.feature.descriptorIndexing &&
            features.variableRateShading == deviceInfo.feature.variableRateShading;
    }

    void VulkanSampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {
        auto &ext = swapChain->GetVkExtent();
        if (ext.width == width && ext.height == height) {
            return;
        }

        swapChain->Resize(width, height);
        ResetFrameBuffer();
    }

    void VulkanSampleBase::ResetFrameBuffer()
    {
        auto imageCount = swapChain->GetImageCount();
        frameBuffers.resize(imageCount);
        colorViews.resize(imageCount);

        vk::FrameBuffer::VkDescriptor fbDesc = {};
        fbDesc.extent                       = swapChain->GetVkExtent();
        fbDesc.pass                         = renderPass;

        vk::ImageView::VkDescriptor viewDesc = {};
        viewDesc.viewType                   = VK_IMAGE_VIEW_TYPE_2D;
        viewDesc.format                     = swapChain->GetVkFormat();

        for (uint32_t i = 0; i < imageCount; ++i) {
            auto image      = swapChain->GetVkImage(i);
            colorViews[i]   = vk::ImageView::CreateImageView(image, viewDesc);
            fbDesc.views    = std::vector<vk::ImageViewPtr>{colorViews[i]};
            frameBuffers[i] = device->CreateDeviceObject<vk::FrameBuffer>(fbDesc);
        }

        imageAvailable = device->CreateDeviceObject<vk::Semaphore>(vk::Semaphore::VkDescriptor{});
        renderFinish   = device->CreateDeviceObject<vk::Semaphore>(vk::Semaphore::VkDescriptor{});
    }

    vk::ShaderPtr VulkanSampleBase::LoadShader(VkShaderStageFlagBits stage, const std::string &path)
    {
        std::vector<uint32_t> spv;
        if (!ReadBin(path, spv)) {
            return {};
        }

        vk::Shader::VkDescriptor shaderDesc = {};
        shaderDesc.stage                   = stage;
        shaderDesc.size                    = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        shaderDesc.spv                     = spv.data();

        return device->CreateDeviceObject<vk::Shader>(shaderDesc);
    }
}
