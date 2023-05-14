
#include "RHISubpassSample.h"
#include "framework/platform/PlatformBase.h"
#include "builder/shader/ShaderCompiler.h"

namespace sky::rhi {

    void RHISubPassSample::SetupBase()
    {
        SetupPass();
        SetupPool();
        RenderPass::Descriptor passDesc = {};
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::STORE});

        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {{0, {AccessFlag::COLOR_WRITE},AspectFlagBit::COLOR_BIT},{1, {AccessFlag::COLOR_WRITE}, AspectFlagBit::COLOR_BIT}}, {}, {},
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {{0, {AccessFlag::COLOR_INOUT_WRITE}}, {1, {AccessFlag::COLOR_INOUT_WRITE}}, {2, {AccessFlag::COLOR_WRITE}}},
                {},
                {{0, {AccessFlag::COLOR_INOUT_WRITE}}, {1, {AccessFlag::COLOR_INOUT_WRITE}}},
        });

        passDesc.dependencies.emplace_back(RenderPass::Dependency{{0}, {1},
                                           {AccessFlag::COLOR_WRITE},
                                           {AccessFlag::COLOR_INOUT_READ}});

        tiedPass = device->CreateRenderPass(passDesc);

        auto &ext = swapChain->GetExtent();
        rhi::Image::Descriptor imageDesc = {};
        imageDesc.format      = PixelFormat::RGBA8_UNORM;
        imageDesc.extent      = {ext.width, ext.height, 1};
        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = tiedPass;

        for (uint32_t i = 0; i < 3; ++i)
        {
            if (i == 2) {
                imageDesc.usage = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
            } else {
                imageDesc.usage = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::INPUT_ATTACHMENT | ImageUsageFlagBit::TRANSIENT;
            }

            auto image = device->CreateImage(imageDesc);
            auto view = image->CreateView({});
            fbDesc.views.emplace_back(view);
            subpassViews.emplace_back(view);
            fbClears.emplace_back(rhi::ClearValue(0, 0, 0, 1));
        }
        fb = device->CreateFrameBuffer(fbDesc);

        // pipeline layouts
        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{
                DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 0, ShaderStageFlagBit::FS, "colorOut"
            });
            auto localLayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(localLayout);

            DescriptorSet::Descriptor setDesc = { localLayout };
            fullScreenSet = pool->Allocate(setDesc);
            fullScreenSet->BindImageView(0, fbDesc.views[2], 0);
            fullScreenSet->Update();

            fullScreenLayout = device->CreatePipelineLayout(pLayoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 0, ShaderStageFlagBit::FS, "inColor0"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 1, ShaderStageFlagBit::FS, "inColor1"});
            auto passlayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(passlayout);

            DescriptorSet::Descriptor setDesc = { passlayout };
            subpassSet = pool->Allocate(setDesc);
            subpassSet->BindImageView(0, fbDesc.views[0], 0, DescriptorBindFlagBit::FEEDBACK_LOOP);
            subpassSet->BindImageView(1, fbDesc.views[1], 0, DescriptorBindFlagBit::FEEDBACK_LOOP);
            subpassSet->Update();

            subpassLayout = device->CreatePipelineLayout(pLayoutDesc);
        }

        // shaders
        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/full_screen_vs.glsl", {path + "/shaders/RHISample/full_screen_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/full_screen_fs.glsl", {path + "/shaders/RHISample/full_screen_fs.shader", builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub01_fs.glsl", {path + "/shaders/RHISample/sub01_fs.shader", builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub02_fs.glsl", {path + "/shaders/RHISample/sub02_fs.shader", builder::ShaderType::FS});

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/full_screen_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = fullScreenLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.subPassIndex = 0;
        fullScreen = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.renderPass = tiedPass;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub01_fs.shader");
        psoDesc.subPassIndex = 0;

        pso1 = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.pipelineLayout = subpassLayout;
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub02_fs.shader");
        psoDesc.subPassIndex = 1;

        pso2 = device->CreateGraphicsPipeline(psoDesc);
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

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::NONE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::COLOR_WRITE);
            barrier.view = subpassViews[0];
            commandBuffer->QueueBarrier(barrier);
            barrier.view = subpassViews[1];
            commandBuffer->QueueBarrier(barrier);
            barrier.view = subpassViews[2];
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        encoder->BeginPass({fb, tiedPass, static_cast<uint32_t>(fbClears.size()), fbClears.data()});

        encoder->BindPipeline(pso1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->NextSubPass();

        encoder->BindPipeline(pso2);
        encoder->BindSet(0, subpassSet);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::COLOR_WRITE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::FRAGMENT_SRV);
            barrier.view = subpassViews[2];
            commandBuffer->QueueBarrier(barrier);
        }

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::NONE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::COLOR_WRITE);
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
        }

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::NONE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::DEPTH_STENCIL_READ);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::DEPTH_STENCIL_WRITE);
            barrier.view = depthStencilImage;
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        encoder->BeginPass({frameBuffers[index], renderPass, static_cast<uint32_t>(clears.size()), clears.data()});

        encoder->BindPipeline(fullScreen);
        encoder->BindSet(0, fullScreenSet);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::COLOR_WRITE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::PRESENT);
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
            commandBuffer->FlushBarriers();
        }

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }

    void RHISubPassSample::OnStop()
    {
        device->WaitIdle();
        tiedPass = nullptr;
        fb = nullptr;
        pso1 = nullptr;
        pso2 = nullptr;
        subpassSet = nullptr;
        subpassLayout = nullptr;
        subpassViews.clear();

        fullScreen = nullptr;
        fullScreenLayout = nullptr;
        fullScreenSet = nullptr;
        RHISampleBase::OnStop();
    }

} // namespace sky::rhi
