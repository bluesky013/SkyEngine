//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <GLESForward.h>
#include <memory>

namespace sky::aurora {

    class GLESInstance;

    struct GLESContext : ThreadContext {
        explicit GLESContext(GLESInstance &inst);
        ~GLESContext();

        void OnAttach(uint32_t threadIndex) override;
        void OnDetach() override;

        GLESInstance &instance;
        EGLContext    eglContext = EGL_NO_CONTEXT;
        EGLSurface    pbuffer   = EGL_NO_SURFACE;
    };

    class GLESDevice : public Device {
    public:
        explicit GLESDevice(GLESInstance &inst);
        ~GLESDevice() override;

        Fence *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer* CreateBuffer(const Buffer::Descriptor &desc) override;
        Image* CreateImage(const Image::Descriptor &desc) override;
        Sampler* CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroup* CreateSampler(const ResourceGroup::Descriptor &desc) override { return nullptr; }
        SwapChain* CreateSwapChain(const SwapChain::Descriptor &desc) override;

        ShaderFunction* CreateShaderFunction(const ShaderFunction::Descriptor &desc) override;
        Shader* CreateShader(const Shader::Descriptor &desc) override;
        GraphicsPipeline* CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override;
        ComputePipeline* CreatePipelineState(const ComputePipeline::Descriptor &desc) override { return nullptr; }

        GLESInstance &GetInstance() const { return instance; }

    private:
        ThreadContext* CreateAsyncContext() override;
        bool OnInit(const DeviceInit& init) override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        GLESInstance &instance;
    };

} // namespace sky::aurora
