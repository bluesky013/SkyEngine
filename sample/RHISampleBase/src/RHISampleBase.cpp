//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"

namespace sky::rhi {

    void RHISampleBase::OnStart()
    {
        instance = Instance::Create({"", "", false, API::GLES});
        device   = instance->CreateDevice({});

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
        SwapChain::Descriptor swcDesc = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swcDesc.width  = nativeWindow->GetWidth();
        swcDesc.height = nativeWindow->GetHeight();
        swapChain      = device->CreateSwapChain(swcDesc);

        auto format = swapChain->GetFormat();
        auto &ext = swapChain->GetExtent();
        uint32_t count = swapChain->GetImageCount();

        rhi::RenderPass::Descriptor passDesc = {};
        passDesc.attachments.emplace_back(RenderPass::Attachment{
            format,
            SampleCount::X1,
            LoadOp::CLEAR,
            StoreOp::STORE,
            LoadOp::DONT_CARE,
            StoreOp::DONT_CARE,
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
            {0}, {}, {}, ~(0U)
        });
        renderPass = device->CreateRenderPass(passDesc);

        rhi::FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = renderPass;
        frameBuffers.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            fbDesc.views.emplace_back(swapChain->GetImage(i)->CreateView({}));
            frameBuffers.emplace_back(device->CreateFrameBuffer(fbDesc));
        }

        rhi::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = device->CreateCommandBuffer(cmdDesc);
    }

    void RHISampleBase::OnStop()
    {
        swapChain = nullptr;

        delete device;
        device = nullptr;

        Instance::Destroy(instance);
        instance = nullptr;
    }

    void RHISampleBase::OnTick(float delta)
    {
        rhi::ClearValue clear = {};
        clear.color.float32[0] = 1.f;
        clear.color.float32[1] = 0.f;
        clear.color.float32[2] = 0.f;
        clear.color.float32[3] = 1.f;

        auto queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage();
        commandBuffer->Begin();
        commandBuffer->EncodeGraphics()->BeginPass(frameBuffers[index], renderPass, 1, &clear)
            .EndPass();
        commandBuffer->End();
        commandBuffer->Submit(*queue);
    }

    void RHISampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }
}
