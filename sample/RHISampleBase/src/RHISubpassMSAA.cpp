//
// Created by Zach Lee on 2023/6/14.
//

#include <RHISubpassMSAA.h>
#include "framework/platform/PlatformBase.h"
#include "builder/shader/ShaderCompiler.h"

namespace sky::rhi {

    void RHISubPassMSAA::SetupBase() {
        SetupPass();
        SetupPool();
        RenderPass::Descriptor passDesc = {};

        auto sample1 = SampleCount::X4;
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM, sample1,         LoadOp::CLEAR, StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM, SampleCount::X1, LoadOp::CLEAR, StoreOp::STORE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM, sample1,         LoadOp::CLEAR, StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{PixelFormat::RGBA8_UNORM, SampleCount::X1, LoadOp::CLEAR, StoreOp::STORE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{dsFormat, sample1,         LoadOp::CLEAR, StoreOp::DONT_CARE, LoadOp::CLEAR, StoreOp::DONT_CARE});
        passDesc.attachments.emplace_back(RenderPass::Attachment{dsFormat, SampleCount::X1, LoadOp::CLEAR, StoreOp::STORE, LoadOp::CLEAR, StoreOp::STORE});

        passDesc.subPasses.emplace_back(RenderPass::SubPass{
            {
                {0, AccessFlagBit::COLOR_WRITE},
                {2, AccessFlagBit::COLOR_WRITE},
            },
            {
                {1, AccessFlagBit::COLOR_WRITE},
                {3, AccessFlagBit::COLOR_WRITE},
            },
            {},
            {},
            {   4, AccessFlagBit::DEPTH_STENCIL_WRITE},
            {   5, AccessFlagBit::DEPTH_STENCIL_WRITE},
        });

        tiedPass = device->CreateRenderPass(passDesc);

        auto count = swapChain->GetImageCount();
        const auto &ext = swapChain->GetExtent();

        rhi::Image::Descriptor desc = {};
        desc.imageType = ImageType::IMAGE_2D;
        desc.format = PixelFormat::RGBA8_UNORM;
        desc.extent = {ext.width, ext.height, 1};
        desc.mipLevels = 1;
        desc.arrayLayers = 1;
        desc.memory = MemoryType::GPU_ONLY;

        rhi::ImageViewDesc viewDesc = {};
        {
            desc.samples = sample1;
            desc.usage = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSIENT;
            {
                auto image = device->CreateImage(desc);
                ms1 = image->CreateView(viewDesc);
            }
            {
                auto image = device->CreateImage(desc);
                ms2 = image->CreateView(viewDesc);
            }
        }
        {
            desc.samples = rhi::SampleCount::X1;
            desc.usage =
                ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::INPUT_ATTACHMENT | ImageUsageFlagBit::SAMPLED;
            {
                auto image = device->CreateImage(desc);
                resolve1 = image->CreateView(viewDesc);
            }
            {
                auto image = device->CreateImage(desc);
                resolve2 = image->CreateView(viewDesc);
            }
        }
        {
            desc.samples = sample1;
            desc.format = dsFormat;
            desc.usage = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::TRANSIENT;

            viewDesc.subRange.aspectMask = AspectFlagBit::DEPTH_BIT | AspectFlagBit::STENCIL_BIT;
            {
                auto image = device->CreateImage(desc);
                ds = image->CreateView(viewDesc);
            }

            desc.usage = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::SAMPLED;
            {
                desc.samples = rhi::SampleCount::X1;
                auto image = device->CreateImage(desc);
                dsResolve = image->CreateView(viewDesc);
            }
        }

        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = tiedPass;
        fbDesc.views.resize(6);
        fbDesc.views[0] = ms1;
        fbDesc.views[1] = resolve1;
        fbDesc.views[2] = ms2;
        fbDesc.views[3] = resolve2;
        fbDesc.views[4] = ds;
        fbDesc.views[5] = dsResolve;
        fb = device->CreateFrameBuffer(fbDesc);

        fbClears.resize(6, ClearValue(0, 0, 0, 0));
        fbClears[4] = ClearValue(1.0, 0);
        fbClears[5] = ClearValue(1.0, 0);

        // pipeline layouts
        {
            PipelineLayout::Descriptor pLayoutDesc = {};
            emptyLayout = device->CreatePipelineLayout(pLayoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(
                DescriptorSetLayout::SetBinding{DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 0, ShaderStageFlagBit::FS, "colorImage1"});
            layoutDesc.bindings.emplace_back(
                DescriptorSetLayout::SetBinding{DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 1, ShaderStageFlagBit::FS, "colorImage2"});
            auto passlayout = device->CreateDescriptorSetLayout(layoutDesc);
            PipelineLayout::Descriptor pLayoutDesc = {};
            pLayoutDesc.layouts.emplace_back(passlayout);
            DescriptorSet::Descriptor setDesc = {passlayout};
            set1 = pool->Allocate(setDesc);
            set1->BindImageView(0, resolve1, 0);
            set1->BindImageView(1, resolve2, 0);
            set1->Update();
            fullScreenLayout = device->CreatePipelineLayout(pLayoutDesc);
        }


        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/full_screen_vs.glsl",
                                               {path + "/shaders/RHISample/full_screen_vs.shader",
                                                builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/full_screen_fs_2.glsl",
                                               {path + "/shaders/RHISample/full_screen_fs_2.shader",
                                                builder::ShaderType::FS});
        builder::ShaderCompiler::CompileShader("shaders/triangle_msaa_vs.glsl",
                                               {path + "/shaders/RHISample/triangle_msaa_vs.shader",
                                                builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/triangle_msaa_fs.glsl",
                                               {path + "/shaders/RHISample/triangle_msaa_fs.shader",
                                                builder::ShaderType::FS});

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.rasterState.cullMode = rhi::CullModeFlagBits::NONE;
        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.state.multiSample.sampleCount = rhi::SampleCount::X1;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS,
                                  path + "/shaders/RHISample/full_screen_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS,
                                  path + "/shaders/RHISample/full_screen_fs_2.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = fullScreenLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.subPassIndex = 0;
        pso2 = device->CreateGraphicsPipeline(psoDesc);

        psoDesc.state.blendStates.emplace_back(BlendState{});
        psoDesc.state.multiSample.sampleCount = sample1;
        psoDesc.state.depthStencil.depthWrite = true;
        psoDesc.state.depthStencil.depthTest = true;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS,
                                  path + "/shaders/RHISample/triangle_msaa_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS,
                                  path + "/shaders/RHISample/triangle_msaa_fs.shader");
        psoDesc.renderPass = tiedPass;
        psoDesc.pipelineLayout = emptyLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.subPassIndex = 0;
        pso1 = device->CreateGraphicsPipeline(psoDesc);
    }

    void RHISubPassMSAA::OnTick(float delta)
    {
        auto *queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage(imageAvailable);

        SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            PipelineStageBit::COLOR_OUTPUT, imageAvailable);
        submitInfo.fence = fence;

        fence->WaitAndReset();
        commandBuffer->Begin();

        auto encoder = commandBuffer->EncodeGraphics();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.view = ms1;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = ms2;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = resolve1;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = resolve2;
            commandBuffer->QueueBarrier(barrier);
        }
        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::DEPTH_STENCIL_WRITE;
            barrier.view = ds;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = dsResolve;
            commandBuffer->QueueBarrier(barrier);
        }
        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
        }
        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::NONE;
            barrier.dstFlags = rhi::AccessFlagBit::DEPTH_STENCIL_WRITE;
            barrier.view = depthStencilImage;
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        encoder->BeginPass({fb, tiedPass, static_cast<uint32_t>(fbClears.size()), fbClears.data()});

        encoder->BindPipeline(pso1);
        encoder->DrawLinear({3, 1, 0, 0});

        encoder->EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.dstFlags = rhi::AccessFlagBit::FRAGMENT_SRV;
            barrier.view = resolve1;
            commandBuffer->QueueBarrier(barrier);
            barrier.view = resolve2;
            commandBuffer->QueueBarrier(barrier);
        }
        commandBuffer->FlushBarriers();

        commandBuffer->EncodeGraphics()->BeginPass({frameBuffers[index], renderPass, 2, clears.data()});
        encoder->BindPipeline(pso2);
        encoder->BindSet(0, set1);
        encoder->DrawLinear({3, 1, 0, 0});
        encoder->EndPass();

        {
            ImageBarrier barrier = {};
            barrier.srcFlags = rhi::AccessFlagBit::COLOR_WRITE;
            barrier.dstFlags = rhi::AccessFlagBit::PRESENT;
            barrier.view = colorViews[index];
            commandBuffer->QueueBarrier(barrier);
            commandBuffer->FlushBarriers();
        }

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.semaphores.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }

    void RHISubPassMSAA::OnStop()
    {
        device->WaitIdle();
        tiedPass = nullptr;
        fb = nullptr;
        fbClears.clear();

        ms1 = nullptr;
        ms2 = nullptr;
        resolve1 = nullptr;
        resolve2 = nullptr;
        ds = nullptr;
        dsResolve = nullptr;
        pso1 = nullptr;
        pso2 = nullptr;
        fullScreenLayout = nullptr;
        emptyLayout = nullptr;
        set1 = nullptr;
        RHISampleBase::OnStop();
    }


} // namespace sky::rhi
