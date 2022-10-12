//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <Triangle.h>
#include <core/file/FileIO.h>

namespace sky {

    void Triangle::Init()
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
    }

    void Triangle::Start()
    {
        if (device == nullptr) {
            return;
        }

        auto                       nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        drv::SwapChain::Descriptor swcDesc      = {};
        swcDesc.window                          = nativeWindow->GetNativeHandle();
        swapChain                               = device->CreateDeviceObject<drv::SwapChain>(swcDesc);

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

        LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.vert.spv");
        LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.frag.spv");

        vertexInput = drv::VertexInput::Builder().Begin().Build();

        drv::PipelineLayout::Descriptor pDesc = {};
        pipelineLayout                        = device->CreateDeviceObject<drv::PipelineLayout>(pDesc);

        drv::GraphicsPipeline::Program program;
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        drv::GraphicsPipeline::State state = {};
        state.raster.cullMode = VK_CULL_MODE_NONE;
        state.blends.blendStates.emplace_back(drv::GraphicsPipeline::BlendState{});

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.program                           = &program;
        psoDesc.state                             = &state;
        psoDesc.pipelineLayout                    = pipelineLayout;
        psoDesc.vertexInput                       = vertexInput;
        psoDesc.renderPass                        = renderPass;
        pso                                       = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);

        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag                         = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex             = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer                          = commandPool->Allocate(cmdDesc);

        Event<IWindowEvent>::Connect(swcDesc.window, this);
    }

    void Triangle::Stop()
    {
        device->WaitIdle();
        Event<IWindowEvent>::DisConnect(this);
    }

    void Triangle::Tick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        drv::CmdDraw args         = {};
        args.type                 = drv::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 3;
        args.linear.instanceCount = 1;

        drv::DrawItem item = {};
        item.pso           = pso;
        item.drawArgs      = args;

        drv::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.Encode(item);
        graphicsEncoder.EndPass();

        commandBuffer->End();

        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                  = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
    }

    void Triangle::ResetFrameBuffer()
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

    void Triangle::LoadShader(VkShaderStageFlagBits stage, const std::string &path)
    {
        std::vector<uint32_t> spv;
        if (!ReadBin(path, spv)) {
            return;
        }

        drv::Shader::Descriptor shaderDesc = {};
        shaderDesc.stage                   = stage;
        shaderDesc.size                    = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        shaderDesc.spv                     = spv.data();

        auto shader = device->CreateDeviceObject<drv::Shader>(shaderDesc);
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            vs = shader;
        } else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            fs = shader;
        }
    }

    void Triangle::OnWindowResize(uint32_t width, uint32_t height)
    {
        auto &ext = swapChain->GetExtent();
        if (ext.width == width && ext.height == height) {
            return;
        }

        swapChain->Resize(width, height);
        ResetFrameBuffer();
    }

} // namespace sky