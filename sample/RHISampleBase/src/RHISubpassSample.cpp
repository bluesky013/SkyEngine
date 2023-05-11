
#include "RHISubpassSample.h"
#include "framework/platform/PlatformBase.h"
#include "builder/shader/ShaderCompiler.h"

namespace sky::rhi {

    void RHISubPassSample::SetupBase()
    {
        SetupPass();
        RenderPass::Descriptor passDesc = {};
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGB8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGB8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGB8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::STORE});

        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {0, 1}, {}, {},
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {0, 1, 2}, {}, {0, 1},
        });
        tiedPass = device->CreateRenderPass(passDesc);

        auto &ext = swapChain->GetExtent();
        rhi::Image::Descriptor imageDesc = {};
        imageDesc.format      = PixelFormat::RGBA8_UNORM;
        imageDesc.extent      = {ext.width, ext.height, 1};
        imageDesc.usage       = ImageUsageFlagBit::RENDER_TARGET;
        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = tiedPass;

        ImageViewPtr colorOut;
        for (uint32_t i = 0; i < 3; ++i)
        {
            auto image = device->CreateImage(imageDesc);
            auto view = image->CreateView({});
            fbDesc.views.emplace_back(view);
            fbClears.emplace_back(rhi::ClearValue(0, 0, 0, 1));

            if (i == 2) {
                colorOut = view;
            }
        }
        fb = device->CreateFrameBuffer(fbDesc);

        // shaders
        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/full_screen_vs.glsl", {path + "/shaders/RHISample/full_screen_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/full_screen_fs.glsl", {path + "/shaders/RHISample/full_screen_fs.shader", builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub01_fs.glsl", {path + "/shaders/RHISample/sub01_fs.shader", builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub02_fs.glsl", {path + "/shaders/RHISample/sub02_fs.shader", builder::ShaderType::FS});

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.renderPass = tiedPass;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub01_fs.shader");
        psoDesc.subPassIndex = 0;

        pso1 = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub02_fs.shader");
        psoDesc.subPassIndex = 1;

        pso2 = device->CreateGraphicsPipeline(psoDesc);

        // pipeline layout
        DescriptorSetLayout::Descriptor layoutDesc = {};
        layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{
                DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 0, ShaderStageFlagBit::FS, "colorOut"
        });
        auto localLayout = device->CreateDescriptorSetLayout(layoutDesc);

        PipelineLayout::Descriptor pLayoutDesc = {};
        pLayoutDesc.layouts.emplace_back(localLayout);

        fullScreenLayout = device->CreatePipelineLayout(pLayoutDesc);

        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/full_screen_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = fullScreenLayout;
        psoDesc.subPassIndex = 0;
        fullScreen = device->CreateGraphicsPipeline(psoDesc);

        DescriptorSet::Descriptor setDesc = { localLayout };
        fullScreenSet = device->CreateDescriptorSet(setDesc);
        fullScreenSet->BindImageView(0, colorOut, 0);
        fullScreenSet->Update();
    }

    void RHISubPassSample::OnTick(float delta)
    {
        auto queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage(imageAvailable);

        SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
                std::pair<PipelineStageFlags , SemaphorePtr>{PipelineStageBit::COLOR_OUTPUT, imageAvailable});

        commandBuffer->Begin();

        auto encoder = commandBuffer->EncodeGraphics();
        encoder->BeginPass({fb, tiedPass, static_cast<uint32_t>(fbClears.size()), fbClears.data()});

        encoder->BindPipeline(pso1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->NextSubPass();

        encoder->BindPipeline(pso2);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        encoder->BeginPass({frameBuffers[index], renderPass, static_cast<uint32_t>(clears.size()), clears.data()});

        encoder->BindPipeline(fullScreen);
        encoder->BindSet(0, fullScreenSet);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }

    void RHISubPassSample::OnStop()
    {
        tiedPass = nullptr;
        fb = nullptr;
        pso1 = nullptr;
        pso2 = nullptr;
        fullScreen = nullptr;
        fullScreenLayout = nullptr;
        RHISampleBase::OnStop();
    }

} // namespace sky::rhi
