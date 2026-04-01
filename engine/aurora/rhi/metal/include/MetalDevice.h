//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Device.h>

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

        Fence *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer *CreateBuffer(const Buffer::Descriptor &desc) override;
        Image *CreateImage(const Image::Descriptor &desc) override;
        Sampler *CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroup *CreateSampler(const ResourceGroup::Descriptor &desc) override { return nullptr; }
        SwapChain *CreateSwapChain(const SwapChain::Descriptor &desc) override;

        ShaderFunction *CreateShaderFunction(const ShaderFunction::Descriptor &desc) override;
        Shader *CreateShader(const Shader::Descriptor &desc) override;
        GraphicsPipeline *CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override;
        ComputePipeline *CreatePipelineState(const ComputePipeline::Descriptor &desc) override;

        void *GetNativeDevice() const { return metalDevice; }
        void *GetCommandQueue() const { return commandQueue; }
        MetalInstance &GetInstance() const { return instance; }

    private:
        ThreadContext *CreateAsyncContext() override;
        bool OnInit(const DeviceInit &init) override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        MetalInstance &instance;
        void *metalDevice  = nullptr;
        void *commandQueue = nullptr;
    };

} // namespace sky::aurora