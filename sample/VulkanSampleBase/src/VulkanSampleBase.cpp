//
// Created by Zach Lee on 2022/10/13.
//

#include "VulkanSampleBase.h"
#include <core/file/FileIO.h>

namespace sky {

    void VulkanSampleBase::Init()
    {
        drv::Driver::Descriptor drvDes = {};
        drvDes.enableDebugLayer        = true;
        drvDes.appName                 = "Triangle";
        driver                         = drv::Driver::Create(drvDes);
        if (driver == nullptr) {
            return;
        }

        drv::Device::Descriptor devDes = {};
        device                         = driver->CreateDevice(devDes);

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);
    }

    void VulkanSampleBase::Start()
    {
        auto                       nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        drv::SwapChain::Descriptor swcDesc      = {};
        swcDesc.window                          = nativeWindow->GetNativeHandle();
        swapChain                               = device->CreateDeviceObject<drv::SwapChain>(swcDesc);
        Event<IWindowEvent>::Connect(swcDesc.window, this);

        renderPass = drv::RenderPassFactory()()
                         .AddSubPass()
                         .AddColor()
                         .Format(swapChain->GetFormat())
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

        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag                         = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex             = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer                          = commandPool->Allocate(cmdDesc);

        OnStart();
    }

    void VulkanSampleBase::Stop()
    {
        OnStop();
        device->WaitIdle();
        Event<IWindowEvent>::DisConnect(this);
    }

    void VulkanSampleBase::Tick(float delta)
    {
        frameIndex = (frameIndex + 1) % 2;
    }

    void VulkanSampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {
        auto &ext = swapChain->GetExtent();
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

        drv::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent                       = swapChain->GetExtent();
        fbDesc.pass                         = renderPass;

        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.viewType                   = VK_IMAGE_VIEW_TYPE_2D;
        viewDesc.format                     = swapChain->GetFormat();

        for (uint32_t i = 0; i < imageCount; ++i) {
            auto image      = swapChain->GetImage(i);
            colorViews[i]   = drv::ImageView::CreateImageView(image, viewDesc);
            fbDesc.views    = std::vector<drv::ImageViewPtr>{colorViews[i]};
            frameBuffers[i] = device->CreateDeviceObject<drv::FrameBuffer>(fbDesc);
        }

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish   = device->CreateDeviceObject<drv::Semaphore>({});
    }

    drv::ShaderPtr VulkanSampleBase::LoadShader(VkShaderStageFlagBits stage, const std::string &path)
    {
        std::vector<uint32_t> spv;
        if (!ReadBin(path, spv)) {
            return {};
        }

        drv::Shader::Descriptor shaderDesc = {};
        shaderDesc.stage                   = stage;
        shaderDesc.size                    = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        shaderDesc.spv                     = spv.data();

        return device->CreateDeviceObject<drv::Shader>(shaderDesc);
    }
}