//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <rhi/Device.h>
#include <rhi/Instance.h>
#include <rhi/Swapchain.h>
#include <rhi/RenderPass.h>
#include <rhi/FrameBuffer.h>
#include <rhi/CommandBuffer.h>
#include <rhi/QueryPool.h>
#include <IRHI.h>

namespace sky {
    class NativeWindow;
}

namespace sky::rhi {

    ShaderPtr CreateShader(API api, Device &device, ShaderStageFlagBit stage, const std::string &path);

    class RHISampleBase : public IWindowEvent, public IRHI {
    public:
        RHISampleBase() = default;
        ~RHISampleBase() override = default;

        virtual void OnStart();
        virtual void OnStop();
        virtual void OnTick(float delta);
        void SetAPI(API api) { rhi = api; }

        virtual void SetFeature() {}
        bool CheckFeature() const;
    protected:
        void OnWindowResize(uint32_t width, uint32_t height) override;

        virtual void SetupBase();

        void SetupPool();
        void SetupPass();
        void SetupTriangle();
        void SetupQueryPool();
        void ResetFramebuffer();
        rhi::Device * GetDevice() const override { return device; }

        rhi::Instance *instance = nullptr;
        rhi::Device *device = nullptr;

        rhi::DescriptorSetPoolPtr pool;
        rhi::SwapChainPtr swapChain;
        rhi::RenderPassPtr renderPass;
        std::vector<rhi::ImageViewPtr> colorViews;
        std::vector<rhi::FrameBufferPtr> frameBuffers;
        rhi::FencePtr fence;
        rhi::CommandBufferPtr commandBuffer;
        rhi::GraphicsPipelinePtr pso;
        rhi::PipelineLayoutPtr pipelineLayout;
        rhi::VertexInputPtr emptyInput;
        rhi::ImageViewPtr depthStencilImage;
        std::vector<ClearValue> clears;
        const NativeWindow* window = nullptr;

        rhi::SemaphorePtr imageAvailable;
        rhi::SemaphorePtr renderFinish;
        rhi::Device::DeviceFeature deviceFeature = {};

        rhi::QueryPoolPtr psPool;
        rhi::BufferPtr psResult;
        uint32_t psResultStride = 0;

        rhi::QueryPoolPtr timeStampPool;
        rhi::BufferPtr timeStampResult;
        uint32_t timeStampResultStride = 0;
        uint32_t timeStampCount = 4;
        std::vector<uint64_t> timeStampSysResult;

        uint32_t frameIndex = 0;
        uint32_t maxFrameInflight = 2;

        API rhi = API::DEFAULT;
#if __APPLE__
        rhi::PixelFormat dsFormat = rhi::PixelFormat::D32_S8;
#else
        rhi::PixelFormat dsFormat = rhi::PixelFormat::D24_S8;
#endif
    };
}
