//
// Created by Zach Lee on 2023/4/22.
//
#include <perf/render/Render.h>
#include <framework/interface/Interface.h>
#include <framework/interface/ISystem.h>
#include <framework/window/NativeWindow.h>

namespace sky::perf {

    void Render::Init()
    {
        vk::Instance::Descriptor drvDes = {};
        drvDes.enableDebugLayer         = true;
        drvDes.appName                  = "PerfTool";
        instance                        = vk::Instance::Create(drvDes);
        if (instance == nullptr) {
            return;
        }

        vk::Device::Descriptor deviceInfo = {};

        device        = instance->CreateDevice(deviceInfo);
        graphicsQueue = device->GetGraphicsQueue();

        const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        vk::SwapChain::VkDescriptor swcDesc      = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swapChain      = device->CreateDeviceObject<vk::SwapChain>(swcDesc);
        Event<IWindowEvent>::Connect(nativeWindow, this);

        InitRenderPass();
        InitDescriptorSetPool();
        ResetFrameBuffer();

        vk::CommandBuffer::VkDescriptor cmdDesc = {};

        commandBuffer = graphicsQueue->AllocateCommandBuffer(cmdDesc);

        vk::Fence::VkDescriptor fenceDesc = {};
        fenceDesc.flag = VK_FENCE_CREATE_SIGNALED_BIT;
        fence = device->CreateDeviceObject<vk::Fence>(fenceDesc);
    }

    void Render::Stop()
    {
        swapChain = nullptr;
        imageAvailable = nullptr;
        renderFinish = nullptr;
        renderPass = nullptr;
        commandBuffer = nullptr;
        pool = nullptr;
        frameBuffers.clear();
        colorViews.clear();

        delete device;
        vk::Instance::Destroy(instance);
        device = nullptr;
        instance = nullptr;

        Event<IWindowEvent>::DisConnect(this);
    }

    void Render::OnTick()
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeVkGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = frameBuffers[imageIndex];
        beginInfo.renderPass        = renderPass;
        beginInfo.clearValueCount   = 1;
        beginInfo.clearValues       = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        if (encoder) {
            encoder(graphicsEncoder);
        }

        graphicsEncoder.EndPass();

        commandBuffer->End();

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.fence = fence;
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                 = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
    }

    void Render::InitDescriptorSetPool()
    {
        static VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
        };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets = 16;
        poolInfo.num = sizeof(sizes) / sizeof(VkDescriptorPoolSize);
        poolInfo.sizes = sizes;
        pool = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
    }

    void Render::InitRenderPass()
    {
        vk::RenderPass::VkDescriptor passDesc = {};
        passDesc.attachments.emplace_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr, 0,
            swapChain->GetVkFormat(),
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
        passDesc.subPasses.emplace_back(vk::RenderPass::SubPass{
            {{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}}, {}, {},
            {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, ~(0U)},
        });

        passDesc.dependencies.emplace_back(VkSubpassDependency2 {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, nullptr,
            VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0
        });

        passDesc.dependencies.emplace_back(VkSubpassDependency2 {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, nullptr,
            0, VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0
        });
        renderPass = device->CreateDeviceObject<vk::RenderPass>(passDesc);
    }

    void Render::ResetFrameBuffer()
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

    void Render::OnWindowResize(const WindowResizeEvent& event)
    {
        auto &ext = swapChain->GetVkExtent();
        if (ext.width == event.width && ext.height == event.height) {
            return;
        }

        const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        swapChain->Resize(width, height, nativeWindow->GetNativeHandle());
        ResetFrameBuffer();
    }

}
