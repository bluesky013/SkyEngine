
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
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::STORE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::D24_S8,SampleCount::X1,LoadOp::CLEAR,StoreOp::DONT_CARE});

        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {
                    {0, {AccessFlag::COLOR_WRITE}},
                    {1, {AccessFlag::COLOR_WRITE}},
                    {2, {AccessFlag::COLOR_WRITE}},
                    {3, {AccessFlag::COLOR_WRITE}},
                 }, {}, {}, {},
                {5, {AccessFlag::DEPTH_STENCIL_WRITE}},
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {
                        {4, {AccessFlag::COLOR_WRITE}}
                        },
                {},
                {
                        {0, {AccessFlag::COLOR_INPUT}},
                        {1, {AccessFlag::COLOR_INPUT}},
                        },
                { 2, 3 },
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
                {
                        {4, {AccessFlag::COLOR_INOUT_WRITE}}
                        },
                {},
                {
                        {2, {AccessFlag::COLOR_INPUT}},
                        {3, {AccessFlag::COLOR_INPUT}},
                        {4, {AccessFlag::COLOR_INOUT_WRITE}},
                        {5, {AccessFlag::DEPTH_STENCIL_INPUT}, rhi::AspectFlagBit::DEPTH_BIT},
                        {5, {AccessFlag::DEPTH_STENCIL_INPUT}, rhi::AspectFlagBit::STENCIL_BIT},
                        },
        });

        passDesc.dependencies.emplace_back(RenderPass::Dependency{{0}, {1},
                                                                  {AccessFlag::COLOR_WRITE},
                                                                  {AccessFlag::COLOR_INPUT}});
        passDesc.dependencies.emplace_back(RenderPass::Dependency{{0}, {2},
                                                                  {AccessFlag::COLOR_WRITE},
                                                                  {AccessFlag::COLOR_INPUT}});
        passDesc.dependencies.emplace_back(RenderPass::Dependency{{1}, {2},
                                                                  {AccessFlag::COLOR_WRITE},
                                                                  {AccessFlag::COLOR_INOUT_WRITE}});
        tiedPass = device->CreateRenderPass(passDesc);

        auto &ext = swapChain->GetExtent();
        rhi::Image::Descriptor imageDesc = {};
        imageDesc.format      = PixelFormat::RGBA8_UNORM;
        imageDesc.extent      = {ext.width, ext.height, 1};
        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = tiedPass;

        for (uint32_t i = 0; i < 6; ++i)
        {
            rhi::ImageViewDesc viewDesc = {};
            if (i == 5) {
                viewDesc.mask = rhi::AspectFlagBit::DEPTH_BIT | rhi::AspectFlagBit::STENCIL_BIT;
                imageDesc.format = PixelFormat::D24_S8;
                imageDesc.usage = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::INPUT_ATTACHMENT | ImageUsageFlagBit::TRANSIENT;
                fbClears.emplace_back(rhi::ClearValue(1, 0));
            } else if (i == 4) {
                imageDesc.usage = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::INPUT_ATTACHMENT | ImageUsageFlagBit::SAMPLED;
                fbClears.emplace_back(rhi::ClearValue(0, 0, 0, 1));
            } else {
                imageDesc.usage = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::INPUT_ATTACHMENT | ImageUsageFlagBit::TRANSIENT;
                fbClears.emplace_back(rhi::ClearValue(0, 0, 0, 1));
            }

            auto image = device->CreateImage(imageDesc);
            auto view = image->CreateView(viewDesc);
            fbDesc.views.emplace_back(view);
            subpassViews.emplace_back(view);

            if (i == 5) {
                rhi::ImageViewDesc dsViewDesc = {};
                dsViewDesc.mask = AspectFlagBit::DEPTH_BIT;
                depthView = image->CreateView(dsViewDesc);
                dsViewDesc.mask = AspectFlagBit::STENCIL_BIT;
                stencilView = image->CreateView(dsViewDesc);
            }
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
            fullScreenSet->BindImageView(0, fbDesc.views[4], 0);
            fullScreenSet->Update();

            fullScreenLayout = device->CreatePipelineLayout(pLayoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 0, ShaderStageFlagBit::FS,"inColor0"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 1, ShaderStageFlagBit::FS,"inColor1"});
            auto passlayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(passlayout);
            DescriptorSet::Descriptor setDesc = {passlayout};
            subpassSet1 = pool->Allocate(setDesc);
            subpassSet1->BindImageView(0, fbDesc.views[0], 0);
            subpassSet1->BindImageView(1, fbDesc.views[1], 0);
            subpassSet1->Update();
            subpassLayout1 = device->CreatePipelineLayout(pLayoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 0, ShaderStageFlagBit::FS,"inColor0"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 1, ShaderStageFlagBit::FS,"inColor1"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 2, ShaderStageFlagBit::FS,"inColor2"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 3, ShaderStageFlagBit::FS,"inDepth"});
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 4, ShaderStageFlagBit::FS,"inStencil"});
            auto passlayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(passlayout);
            DescriptorSet::Descriptor setDesc = {passlayout};
            subpassSet2 = pool->Allocate(setDesc);
            subpassSet2->BindImageView(0, fbDesc.views[2], 0);
            subpassSet2->BindImageView(1, fbDesc.views[3], 0);
            subpassSet2->BindImageView(2, fbDesc.views[4], 0, DescriptorBindFlagBit::FEEDBACK_LOOP);
            subpassSet2->BindImageView(3, depthView, 0);
            subpassSet2->BindImageView(4, stencilView, 0);
            subpassSet2->Update();
            subpassLayout2 = device->CreatePipelineLayout(pLayoutDesc);
        }

        // shaders
        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/full_screen_vs.glsl", {path + "/shaders/RHISample/full_screen_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/full_screen_fs.glsl", {path + "/shaders/RHISample/full_screen_fs.shader", builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub01_vs.glsl", {path + "/shaders/RHISample/sub01_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub01_fs.glsl", {path + "/shaders/RHISample/sub01_fs.shader", builder::ShaderType::FS, tiedPass->GetInputMap(0), tiedPass->GetOutputMap(0)});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub02_fs.glsl", {path + "/shaders/RHISample/sub02_fs.shader", builder::ShaderType::FS, tiedPass->GetInputMap(1), tiedPass->GetOutputMap(1)});
        builder::ShaderCompiler::CompileShader("shaders/subpass/sub03_fs.glsl", {path + "/shaders/RHISample/sub03_fs.shader", builder::ShaderType::FS, tiedPass->GetInputMap(2), tiedPass->GetOutputMap(2)});

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
        psoDesc.pipelineLayout = subpassLayout1;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub02_fs.shader");
        psoDesc.subPassIndex = 1;

        pso2 = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.pipelineLayout = subpassLayout2;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub03_fs.shader");
        psoDesc.subPassIndex = 2;

        pso3 = device->CreateGraphicsPipeline(psoDesc);


        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.state.depthStencil.depthWrite = true;
        psoDesc.state.depthStencil.depthTest = true;
        psoDesc.state.depthStencil.stencilTest = true;
        psoDesc.state.depthStencil.front.reference = 0x80;
        psoDesc.state.depthStencil.front.writeMask = 0xFF;
        psoDesc.state.depthStencil.front.compareMask = 0xFF;
        psoDesc.state.depthStencil.front.passOp = StencilOp::REPLACE;
        psoDesc.state.depthStencil.front.compareOp = CompareOp::ALWAYS;
        psoDesc.state.depthStencil.back = psoDesc.state.depthStencil.front;

        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/sub01_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/sub01_fs.shader");
        psoDesc.subPassIndex = 0;
        pso1 = device->CreateGraphicsPipeline(psoDesc);
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
            barrier.view = subpassViews[3];
            commandBuffer->QueueBarrier(barrier);
            barrier.view = subpassViews[4];
            commandBuffer->QueueBarrier(barrier);
        }
        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::NONE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::DEPTH_STENCIL_WRITE);
            barrier.view = subpassViews[5];
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        encoder->BeginPass({fb, tiedPass, static_cast<uint32_t>(fbClears.size()), fbClears.data()});

        encoder->BindPipeline(pso1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->NextSubPass();

        encoder->BindPipeline(pso2);
        encoder->BindSet(0, subpassSet1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->NextSubPass();

        encoder->BindPipeline(pso3);
        encoder->BindSet(0, subpassSet2);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags.emplace_back(rhi::AccessFlag::COLOR_INOUT_WRITE);
            barrier.dstFlags.emplace_back(rhi::AccessFlag::FRAGMENT_SRV);
            barrier.view = subpassViews[4];
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
        pso3 = nullptr;
        subpassSet1 = nullptr;
        subpassSet2 = nullptr;
        subpassLayout1 = nullptr;
        subpassLayout2 = nullptr;
        subpassViews.clear();
        depthView = nullptr;
        stencilView = nullptr;

        fullScreen = nullptr;
        fullScreenLayout = nullptr;
        fullScreenSet = nullptr;
        RHISampleBase::OnStop();
    }

} // namespace sky::rhi
