//
// Created by Zach Lee on 2022/6/16.
//


#include <FrameGraphSample.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphPass.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <core/file/FileIO.h>
#include <vulkan/Util.h>

namespace sky::render {

    void FrameGraphSample::Init()
    {
        StartInfo info = {};
        info.appName = "FrameGraphSample";
        Render::Get()->Init(info);

        device = DriverManager::Get()->GetDevice();
    }

    void FrameGraphSample::Start()
    {
        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);
        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = commandPool->Allocate(cmdDesc);

        OnWindowResize(1, 1);
        Event<IWindowEvent>::Connect(viewport->GetNativeHandle(), this);

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish = device->CreateDeviceObject<drv::Semaphore>({});

        LoadShader(VK_SHADER_STAGE_VERTEX_BIT, "shaders/Triangle.vert.spv");
        LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/Triangle.frag.spv");

        vertexInput = drv::VertexInput::Builder().Begin()
            .Build();

        drv::PipelineLayout::Descriptor pDesc = {};
        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pDesc);
    }

    void FrameGraphSample::Stop()
    {
        viewport->Shutdown();
        viewport = nullptr;

        Render::Get()->Destroy();
        Event<IWindowEvent>::DisConnect(this);
    }

    void FrameGraphSample::OnWindowResize(uint32_t width, uint32_t height)
    {
        viewport = std::make_unique<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);

        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();

        drv::Image::Descriptor dsDesc = {};
        dsDesc.format = VK_FORMAT_D32_SFLOAT;
        dsDesc.extent.width = ext.width;
        dsDesc.extent.height = ext.height;
        dsDesc.samples = VK_SAMPLE_COUNT_4_BIT;
        dsDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        depthStencil = device->CreateDeviceObject<drv::Image>(dsDesc);

        dsDesc.format = swapChain->GetFormat();
        dsDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        msaaColor = device->CreateDeviceObject<drv::Image>(dsDesc);
    }

    void FrameGraphSample::LoadShader(VkShaderStageFlagBits stage, const std::string& path)
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

    void FrameGraphSample::PrepareFrameGraph(FrameGraph& graph, drv::ImagePtr output)
    {
        auto clearColor = drv::MakeClearColor(0.f, 0.f, 0.f, 0.f);
        auto clearDS = drv::MakeClearDepthStencil(1.f, 0);

        graph.AddPass<FrameGraphEmptyPass>("preparePass", [&](FrameGraphBuilder& builder) {
            builder.ImportImage("ColorMSAAImage", msaaColor);
            builder.ImportImage("ColorResolveImage", output);
            builder.ImportImage("DepthImage", depthStencil);
        });

        auto colorPass = graph.AddPass<FrameGraphGraphicPass>("ColorPass", [&](FrameGraphBuilder& builder) {
            builder.CreateImageAttachment("ColorMSAAImage", "ColorMSAA", VK_IMAGE_ASPECT_COLOR_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearColor);

            builder.CreateImageAttachment("ColorResolveImage", "ColorResolve", VK_IMAGE_ASPECT_COLOR_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearColor);

            builder.CreateImageAttachment("DepthImage", "DepthOutput", VK_IMAGE_ASPECT_DEPTH_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearDS);

            builder.WriteAttachment("ColorMSAA", ImageBindFlag::COLOR);
            builder.WriteAttachment("ColorResolve", ImageBindFlag::COLOR_RESOLVE);
            builder.WriteAttachment("DepthOutput", ImageBindFlag::DEPTH_STENCIL);
        });

        graph.AddPass<FrameGraphEmptyPass>("Present", [&](FrameGraphBuilder& builder) {
            builder.ReadAttachment("ColorResolve", ImageBindFlag::PRESENT);
        });

//        graph.Compile();

        drv::GraphicsPipeline::Program program;
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        drv::GraphicsPipeline::State state = {};
        state.multiSample.samples = VK_SAMPLE_COUNT_4_BIT;

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.program = &program;
        psoDesc.state = &state;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = vertexInput;
        psoDesc.renderPass = colorPass->GetPass();
        pso = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);

        static drv::CmdDraw args = {};
        args.type = drv::CmdDrawType::LINEAR;
        args.linear.firstVertex = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount = 3;
        args.linear.instanceCount = 1;

//        drv::DrawItem item = {};
//        item.pso = pso;
//        item.drawArgs = &args;
//        colorPass->Emplace(item);
    }

    void FrameGraphSample::Tick(float delta)
    {
        uint32_t imageIndex = 0;
        auto swapChain = viewport->GetSwapChain();
        swapChain->AcquireNext(imageAvailable, imageIndex);

        FrameGraph graph;
        PrepareFrameGraph(graph, swapChain->GetImage(imageIndex));

        commandBuffer->Wait();
        DevObjManager::Get()->TickFreeList();

        commandBuffer->Begin();

        graph.Execute(commandBuffer);

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
}

REGISTER_MODULE(sky::render::FrameGraphSample)