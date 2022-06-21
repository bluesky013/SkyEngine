//
// Created by Zach Lee on 2022/6/16.
//


#include <Triangle.h>
#include <core/file/FileIO.h>

namespace sky {

    void Triangle::Init()
    {
        drv::Driver::Descriptor drvDes = {};
        drvDes.enableDebugLayer = true;
        drvDes.appName = "Triangle";
        driver = drv::Driver::Create(drvDes);
        if (driver == nullptr) {
            return;
        }

        drv::Device::Descriptor devDes = {};
        device = driver->CreateDevice(devDes);
    }

    void Triangle::Start()
    {
        if (device == nullptr) {
            return;
        }

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        drv::SwapChain::Descriptor swcDesc = {};
        swcDesc.window = nativeWindow->GetNativeHandle();
        swapChain = device->CreateDeviceObject<drv::SwapChain>(swcDesc);

        renderPass = drv::RenderPassFactory()().AddSubPass()
            .AddColor()
            .Format(swapChain->GetFormat())
            .Layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .ColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .Samples(VK_SAMPLE_COUNT_1_BIT)
            .AddDependency()
            .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
            .SetBarrier({VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT})
            .AddDependency()
            .SetLinkage(0, VK_SUBPASS_EXTERNAL)
            .SetBarrier({VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT})
            .Create(*device);

        auto imageCount = swapChain->GetImageCount();
        frameBuffers.resize(imageCount);
        colorViews.resize(imageCount);

        drv::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = swapChain->GetExtent();
        fbDesc.pass = renderPass;

        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewDesc.format = swapChain->GetFormat();

        for (uint32_t i = 0; i < imageCount; ++i) {
            auto image = swapChain->GetImage(i);
            colorViews[i] = image->CreateImageView(viewDesc);
            fbDesc.views = std::vector<drv::ImageViewPtr> {
                colorViews[i]
            };
            frameBuffers[i] = device->CreateDeviceObject<drv::FrameBuffer>(fbDesc);
        }

        LoadShader(VK_SHADER_STAGE_VERTEX_BIT, "shaders/Triangle.vert.spv");
        LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/Triangle.frag.spv");

        vertexInput = drv::VertexInput::Builder().Begin()
            .Build();

        drv::PipelineLayout::Descriptor pDesc = {};
        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pDesc);

        drv::GraphicsPipeline::Program program;
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        drv::GraphicsPipeline::State state = {};

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.program = &program;
        psoDesc.state = &state;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = vertexInput;
        psoDesc.renderPass = renderPass;
        pso = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);

        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = commandPool->Allocate(cmdDesc);

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish = device->CreateDeviceObject<drv::Semaphore>({});
    }

    void Triangle::Stop()
    {
        device->WaitIdle();
    }

    void Triangle::Tick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd = commandBuffer->GetNativeHandle();

        VkClearValue clearValue = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        VkRect2D renderArea = {};
        auto& ext = swapChain->GetExtent();
        renderArea.extent.width = ext.width;
        renderArea.extent.height = ext.height;
        renderArea.offset.x = 0;
        renderArea.offset.y = 0;

        VkRenderPassBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = renderPass->GetNativeHandle();
        beginInfo.framebuffer = frameBuffers[imageIndex]->GetNativeHandle();
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = &clearValue;
        beginInfo.renderArea = renderArea;

        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0, 0, static_cast<float>(ext.width), static_cast<float>(ext.height), 0.f, 1.f};

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &renderArea);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pso->GetNativeHandle());
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        commandBuffer->End();

        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable
        });

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
    }

    void Triangle::LoadShader(VkShaderStageFlagBits stage, const std::string& path)
    {
        std::vector<uint32_t> spv;
        if (!ReadBin(path, spv)) {
            return;
        }

        drv::Shader::Descriptor shaderDesc = {};
        shaderDesc.stage = stage;
        shaderDesc.size = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        shaderDesc.spv = spv.data();

        auto shader = device->CreateDeviceObject<drv::Shader>(shaderDesc);
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            vs = shader;
        } else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            fs = shader;
        }
    }

}