//
// Created by Zach Lee on 2023/6/14.
//

#include <RHISubpassMSAA.h>
#include "framework/platform/PlatformBase.h"
#include "builder/shader/ShaderCompiler.h"

namespace sky::rhi {

    void RHISubPassMSAA::SetupBase()
    {
        SetupPass();
        SetupPool();
        RenderPass::Descriptor passDesc = {};

        auto sample1 = SampleCount::X8;
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,sample1,LoadOp::CLEAR,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM,SampleCount::X1,LoadOp::DONT_CARE,StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::BGRA8_UNORM,SampleCount::X1,LoadOp::CLEAR,StoreOp::STORE});

        passDesc.subPasses.emplace_back(RenderPass::SubPass {
            {
                {0, {AccessFlag::COLOR_WRITE}},
            },
            {
                {1, {AccessFlag::COLOR_WRITE}},
            },
            {},
            {},
            {},
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
            {
                {2, {AccessFlag::COLOR_WRITE}},
            },
            {},
            {
                {1, {AccessFlag::COLOR_INPUT}},
            },
            {},
            {},
        });

        passDesc.dependencies.emplace_back(RenderPass::Dependency{{0}, {1},
                                                                  {AccessFlag::COLOR_WRITE},
                                                                  {AccessFlag::COLOR_INPUT, AccessFlag::COLOR_WRITE}});
        tiedPass = device->CreateRenderPass(passDesc);

        auto count = swapChain->GetImageCount();
        auto &ext = swapChain->GetExtent();

        rhi::Image::Descriptor desc = {};
        desc.imageType   = ImageType::IMAGE_2D;
        desc.format      = PixelFormat::RGBA8_UNORM;
        desc.extent      = {ext.width, ext.height, 1};
        desc.mipLevels   = 1;
        desc.arrayLayers = 1;
        desc.memory      = MemoryType::GPU_ONLY;

        rhi::ImageViewDesc viewDesc = {};
        {
            desc.samples = sample1;
            desc.usage   = ImageUsageFlagBit::RENDER_TARGET;
            auto image = device->CreateImage(desc);
            ms1 = image->CreateView(viewDesc);
        }
        {
            desc.samples = rhi::SampleCount::X1;
            desc.usage   = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::INPUT_ATTACHMENT;
            auto image = device->CreateImage(desc);
            resolve1 = image->CreateView(viewDesc);
        }

        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = tiedPass;
        fbDesc.views.resize(3);
        fbDesc.views[0] = ms1;
        fbDesc.views[1] = resolve1;

        fbs.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            fbDesc.views[2] = colorViews[i];
            fbs[i] = device->CreateFrameBuffer(fbDesc);
        }

        fbClears.resize(3, ClearValue(0, 0, 0, 0));

        // pipeline layouts
        {
            PipelineLayout::Descriptor pLayoutDesc = {};
            emptyLayout = device->CreatePipelineLayout(pLayoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{DescriptorType::INPUT_ATTACHMENT, 1, 0, ShaderStageFlagBit::FS, "colorImage"});
            auto passlayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(passlayout);
            DescriptorSet::Descriptor setDesc = {passlayout};
            set = pool->Allocate(setDesc);
            set->BindImageView(0, resolve1, 0);
            set->Update();
            fullScreenLayout = device->CreatePipelineLayout(pLayoutDesc);
        }


        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/full_screen_vs.glsl", {path + "/shaders/RHISample/full_screen_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/full_screen_fs_tiled.glsl", {path + "/shaders/RHISample/full_screen_fs_tiled.shader", builder::ShaderType::FS, tiedPass->GetInputMap(1), tiedPass->GetOutputMap(1)});
        builder::ShaderCompiler::CompileShader("shaders/triangle_vs.glsl", {path + "/shaders/RHISample/triangle_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/triangle_fs.glsl", {path + "/shaders/RHISample/triangle_fs.shader", builder::ShaderType::FS});

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.blendStates.emplace_back(BlendState{});

        psoDesc.state.multiSample.sampleCount = sample1;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/triangle_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/triangle_fs.shader");
        psoDesc.renderPass = tiedPass;
        psoDesc.pipelineLayout = emptyLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.subPassIndex = 0;
        pso1 = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.state.multiSample.sampleCount = rhi::SampleCount::X1;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/full_screen_fs_tiled.shader");
        psoDesc.renderPass = tiedPass;
        psoDesc.pipelineLayout = fullScreenLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.subPassIndex = 1;
        pso2 = device->CreateGraphicsPipeline(psoDesc);
    }

    void RHISubPassMSAA::OnTick(float delta)
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
            barrier.view = ms1;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = resolve1;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        encoder->BeginPass({fbs[index], tiedPass, static_cast<uint32_t>(fbClears.size()), fbClears.data()});

        encoder->BindPipeline(pso1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->NextSubPass();
        encoder->BindPipeline(pso2);
        encoder->BindSet(0, set);
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

    void RHISubPassMSAA::OnStop()
    {
        device->WaitIdle();
        tiedPass = nullptr;
        fbs.clear();
        fbClears.clear();

        ms1 = nullptr;
        resolve1 = nullptr;
        pso1 = nullptr;
        pso2 = nullptr;
        fullScreenLayout = nullptr;
        emptyLayout = nullptr;
        set = nullptr;
        RHISampleBase::OnStop();
    }


} // namespace sky::rhi