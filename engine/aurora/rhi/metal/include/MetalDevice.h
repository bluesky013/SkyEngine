//
// Created on 2026/04/02.
//

#pragma once

#include <MetalQueue.h>
#include <array>
#include <aurora/rhi/Device.h>
#include <memory>

namespace sky::aurora {

    class MetalInstance;

    struct MetalThreadContext : ThreadContext {
        MetalThreadContext() = default;
        ~MetalThreadContext() override;

        void OnAttach(uint32_t threadIndex) override;
        void OnDetach() override;

    private:
        void *autoReleasePool = nullptr;
    };

    class MetalDevice : public Device {
    public:
        explicit MetalDevice(MetalInstance &inst);
        ~MetalDevice() override;

        Fence     *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer              *CreateBuffer(const Buffer::Descriptor &desc) override;
        Image               *CreateImage(const Image::Descriptor &desc) override;
        Sampler             *CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroupLayout *CreateResourceGroupLayout(const ResourceGroupLayout::Descriptor &desc) override
        {
            return nullptr;
        }
        ResourceGroup *CreateResourceGroup(const ResourceGroup::Descriptor &desc) override
        {
            return nullptr;
        }
        PipelineLayout *CreatePipelineLayout(const PipelineLayout::Descriptor &desc) override
        {
            return nullptr;
        }
        SwapChain *CreateSwapChain(const SwapChain::Descriptor &desc) override;

        ShaderFunction   *CreateShaderFunction(const ShaderFunction::Descriptor &desc) override;
        Shader           *CreateShader(const Shader::Descriptor &desc) override;
        GraphicsPipeline *CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override;
        ComputePipeline  *CreatePipelineState(const ComputePipeline::Descriptor &desc) override;

        PixelFormatFeatureFlags GetFormatFeatureFlags(PixelFormat format) const override;

        DeviceFrameContext *CreateFrameContext(const DeviceFrameContextInitInfo &info) override;

        Queue       *GetQueue(QueueType type) override;
        CommandPool *CreateCommandPool(QueueType type) override;

        void *GetNativeDevice() const
        {
            return metalDevice;
        }
        // GetCommandQueue returns the GRAPHICS queue's native handle (legacy
        // accessor; new code should go through GetQueue(QueueType::*)).
        void          *GetCommandQueue() const;
        MetalInstance &GetInstance() const
        {
            return instance;
        }

    private:
        bool        OnInit(const DeviceInit &init) override;
        void        UpdateDeviceCaps() override;
        std::string GetDeviceInfo() const override;
        void        WaitIdle() const override;

        MetalInstance &instance;
        void          *metalDevice = nullptr;

        std::array<std::unique_ptr<MetalQueue>, 3> queues; // by QueueType
    };

} // namespace sky::aurora
