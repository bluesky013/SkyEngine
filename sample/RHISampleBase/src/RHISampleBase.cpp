//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"
#include <filesystem>

#include <core/file/FileIO.h>
#include <framework/platform/PlatformBase.h>

#include <rhi/Queue.h>

#include <builder/shader/ShaderCompiler.h>

namespace sky::rhi {

    ShaderPtr CreateShader(API api, Device &device, ShaderStageFlagBit stage, const std::string &path)
    {
        Shader::Descriptor shaderDesc = {};
        shaderDesc.stage = stage;

        std::string shaderPath = path;
        if (api == API::VULKAN) {
            shaderPath += ".spv";
        } else if (api == API::METAL) {
            shaderPath += ".msl";
        } else if (api == API::GLES) {
            shaderPath += ".gles";
        }
        ReadBin(shaderPath, shaderDesc.data);
        return device.CreateShader(shaderDesc);
    }

    void RHISampleBase::OnStart()
    {
        instance = Instance::Create({"", "", true, rhi});
        device   = instance->CreateDevice({});
        compileGLES = rhi == API::GLES;

        auto systemApi = Interface<ISystemNotify>::Get()->GetApi();
        window = systemApi->GetViewport();
        Event<IWindowEvent>::Connect(window, this);
        SwapChain::Descriptor swcDesc = {};

        swcDesc.window = window->GetNativeHandle();
        swcDesc.width  = window->GetWidth();
        swcDesc.height = window->GetHeight();
        swapChain      = device->CreateSwapChain(swcDesc);

        CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = device->CreateCommandBuffer(cmdDesc);

        imageAvailable = device->CreateSema({});
        renderFinish = device->CreateSema({});

        Interface<IRHI>::Get()->Register(*this);
        SetupBase();
    }

    void RHISampleBase::OnStop()
    {
        device->WaitIdle();

        commandBuffer = nullptr;
        pipelineLayout = nullptr;
        pso = nullptr;
        swapChain = nullptr;
        renderPass = nullptr;
        renderFinish = nullptr;
        imageAvailable = nullptr;
        emptyInput = nullptr;
        depthStencilImage = nullptr;
        pool = nullptr;
        colorViews.clear();
        frameBuffers.clear();

        delete device;
        device = nullptr;

        Instance::Destroy(instance);
        instance = nullptr;

        Event<IWindowEvent>::DisConnect(this);
        Interface<IRHI>::Get()->UnRegister();
    }

    void RHISampleBase::OnTick(float delta)
    {
        auto queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage(imageAvailable);

        SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<PipelineStageFlags , SemaphorePtr>{PipelineStageBit::COLOR_OUTPUT, imageAvailable});

        commandBuffer->Begin();

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
        commandBuffer->EncodeGraphics()->BeginPass({frameBuffers[index], renderPass, 2, clears.data()})
            .BindPipeline(pso)
            .DrawLinear({3, 1, 0, 0})
            .EndPass();

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

    void RHISampleBase::SetupTriangle()
    {
        auto path = Platform::Get()->GetInternalPath();
        builder::ShaderCompiler::CompileShader("shaders/triangle_vs.glsl", {path + "/shaders/RHISample/triangle_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("shaders/triangle_fs.glsl", {path + "/shaders/RHISample/triangle_fs.shader", builder::ShaderType::FS});
        GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, path + "/shaders/RHISample/triangle_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, path + "/shaders/RHISample/triangle_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = emptyInput;
        psoDesc.state.depthStencil.depthTest = true;
        psoDesc.state.depthStencil.depthWrite = true;
        psoDesc.state.blendStates.emplace_back(BlendState{});
        pso = device->CreateGraphicsPipeline(psoDesc);
    }

    void RHISampleBase::SetupBase()
    {
        SetupPass();
        SetupPool();
        SetupTriangle();
    }

    void RHISampleBase::SetupPool()
    {
        rhi::DescriptorSetPool::Descriptor poolDesc = {};
        poolDesc.maxSets = 16;
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::COMBINED_IMAGE_SAMPLER, 128});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::STORAGE_IMAGE, 32});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::UNIFORM_BUFFER, 128});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::UNIFORM_BUFFER_DYNAMIC, 128});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::STORAGE_BUFFER, 32});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::STORAGE_BUFFER_DYNAMIC, 32});
        poolDesc.sizes.emplace_back(DescriptorSetPool::PoolSize{DescriptorType::INPUT_ATTACHMENT, 32});
        pool = device->CreateDescriptorSetPool(poolDesc);
    }

    void RHISampleBase::SetupPass()
    {
        auto format = swapChain->GetFormat();
        auto &ext = swapChain->GetExtent();

        RenderPass::Descriptor passDesc = {};
        passDesc.attachments.emplace_back(RenderPass::Attachment{
            format,
            SampleCount::X1,
            LoadOp::CLEAR,
            StoreOp::STORE,
            LoadOp::DONT_CARE,
            StoreOp::DONT_CARE,
        });
        passDesc.attachments.emplace_back(RenderPass::Attachment{
            PixelFormat::D24_S8,
            SampleCount::X1,
            LoadOp::CLEAR,
            StoreOp::DONT_CARE,
            LoadOp::CLEAR,
            StoreOp::DONT_CARE,
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
            {{0, {AccessFlag::COLOR_WRITE}}}, {}, {}, {},
            {1, {AccessFlag::DEPTH_STENCIL_WRITE}}
        });
        renderPass = device->CreateRenderPass(passDesc);

        clears.resize(2);
        clears[0].color.float32[0] = 0.f;
        clears[0].color.float32[1] = 0.f;
        clears[0].color.float32[2] = 0.f;
        clears[0].color.float32[3] = 1.f;

        clears[1].depthStencil.depth = 1.f;
        clears[1].depthStencil.stencil = 0;

        ResetFramebuffer();

        rhi::PipelineLayout::Descriptor pLayoutDesc = {};
        pipelineLayout = device->CreatePipelineLayout(pLayoutDesc);
        emptyInput = device->CreateVertexInput({});
    }

    void RHISampleBase::ResetFramebuffer()
    {
        auto &ext = swapChain->GetExtent();
        uint32_t count = swapChain->GetImageCount();

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.format      = PixelFormat::D24_S8;
        imageDesc.extent      = {ext.width, ext.height, 1};
        imageDesc.usage       = ImageUsageFlagBit::DEPTH_STENCIL;
        auto image = device->CreateImage(imageDesc);

        rhi::ImageViewDesc viewDesc = {};
        viewDesc.mask = rhi::AspectFlagBit::DEPTH_BIT | rhi::AspectFlagBit::STENCIL_BIT;
        depthStencilImage = image->CreateView(viewDesc);

        frameBuffers.resize(count);
        colorViews.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            FrameBuffer::Descriptor fbDesc = {};
            fbDesc.extent = ext;
            fbDesc.pass = renderPass;
            colorViews[i] = swapChain->GetImage(i)->CreateView({});
            fbDesc.views.emplace_back(colorViews[i]);
            fbDesc.views.emplace_back(depthStencilImage);
            frameBuffers[i] = device->CreateFrameBuffer(fbDesc);
        }
    }

    void RHISampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {
        swapChain->Resize(width, height, window->GetNativeHandle());
        ResetFramebuffer();
    }

    bool RHISampleBase::CheckFeature() const
    {
        return true;
    }
}
