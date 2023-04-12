//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"
#include <rhi/Queue.h>
#include <core/file/FileIO.h>
#include <builder/shader/ShaderCompiler.h>
#include <filesystem>

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

        auto systemApi = Interface<ISystemNotify>::Get()->GetApi();
        auto nativeWindow = systemApi->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
        SwapChain::Descriptor swcDesc = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swcDesc.width  = nativeWindow->GetWidth();
        swcDesc.height = nativeWindow->GetHeight();
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
        commandBuffer = nullptr;
        pipelineLayout = nullptr;
        pso = nullptr;
        swapChain = nullptr;
        renderPass = nullptr;
        renderFinish = nullptr;
        imageAvailable = nullptr;
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

        ImageBarrier barrier = {};
        barrier.srcStage = PipelineStageBit::TOP;
        barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
        barrier.srcFlag = AccessFlag::NONE;
        barrier.dstFlag = AccessFlag::COLOR_WRITE;
        barrier.srcQueueFamily = (~0U);
        barrier.dstQueueFamily = (~0U);
        barrier.mask      = rhi::AspectFlagBit::COLOR_BIT;
        barrier.subRange  = {0, 1, 0, 1};
        barrier.image = swapChain->GetImage(index);

        commandBuffer->Begin();

        commandBuffer->QueueBarrier(barrier);
        commandBuffer->FlushBarriers();

        commandBuffer->EncodeGraphics()->BeginPass({frameBuffers[index], renderPass, 2, clears.data()})
            .BindPipeline(pso)
            .DrawLinear({3, 1, 0, 0})
            .EndPass();

        barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
        barrier.dstStage = PipelineStageBit::BOTTOM;
        barrier.srcFlag = AccessFlag::COLOR_WRITE;
        barrier.dstFlag = AccessFlag::PRESENT;
        commandBuffer->QueueBarrier(barrier);
        commandBuffer->FlushBarriers();

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }

    void RHISampleBase::SetupTriangle()
    {
        rhi::PipelineLayout::Descriptor pLayoutDesc = {};
        pipelineLayout = device->CreatePipelineLayout(pLayoutDesc);

        auto vertexInput = device->CreateVertexInput({});

        builder::ShaderCompiler::CompileShader("triangle_vs.glsl", {"shaders/RHISample/triangle_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("triangle_fs.glsl", {"shaders/RHISample/triangle_fs.shader", builder::ShaderType::FS});
        GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.blendStates.emplace_back(rhi::BlendState{});
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, "shaders/RHISample/triangle_vs.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, "shaders/RHISample/triangle_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout = pipelineLayout;
        psoDesc.vertexInput = vertexInput;
        pso = device->CreateGraphicsPipeline(psoDesc);
    }

    void RHISampleBase::SetupBase()
    {
        SetupPass();
        SetupTriangle();
    }

    void RHISampleBase::SetupPass()
    {
        auto format = swapChain->GetFormat();
        auto &ext = swapChain->GetExtent();
        uint32_t count = swapChain->GetImageCount();

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
            {0}, {}, {}, 1
        });
        renderPass = device->CreateRenderPass(passDesc);

        clears.resize(2);
        clears[0].color.float32[0] = 0.f;
        clears[0].color.float32[1] = 0.f;
        clears[0].color.float32[2] = 0.f;
        clears[0].color.float32[3] = 1.f;

        clears[1].depthStencil.depth = 1.f;
        clears[1].depthStencil.stencil = 0;

        frameBuffers.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            FrameBuffer::Descriptor fbDesc = {};
            fbDesc.extent = ext;
            fbDesc.pass = renderPass;
            fbDesc.views.emplace_back(swapChain->GetImage(i)->CreateView({}));
            frameBuffers.emplace_back(device->CreateFrameBuffer(fbDesc));
        }
    }

    void RHISampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }

    bool RHISampleBase::CheckFeature() const
    {
        return true;
    }
}
