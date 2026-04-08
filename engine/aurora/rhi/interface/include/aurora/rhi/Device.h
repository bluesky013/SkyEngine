//
// Created by blues on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <string>
#include <core/async/ThreadPool.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Sampler.h>
#include <aurora/rhi/SwapChain.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/PipelineState.h>

namespace sky::aurora {

    struct DeviceInit {
        uint32_t parallelContextNum = 1;
    };

    struct DeviceCapability {
        uint32_t maxThreads = 1;

        bool anisotropyEnable = false;
    };

    using DeviceParallelPool = ThreadPool;

    class Device {
    public:
        Device() = default;
        virtual ~Device() = default;

        bool Init();
        void Shutdown();

        virtual std::string GetDeviceInfo() const { return ""; }
        virtual void WaitIdle() const = 0;

        // sync object
        virtual Fence *CreateFence(const Fence::Descriptor &desc) = 0;
        virtual Semaphore *CreateSema(const Semaphore::Descriptor &desc) = 0;

        // hardware resource
        virtual Buffer* CreateBuffer(const Buffer::Descriptor &desc) = 0;
        virtual Image* CreateImage(const Image::Descriptor &desc) = 0;
        virtual Sampler* CreateSampler(const Sampler::Descriptor &desc) = 0;
        virtual ResourceGroup* CreateSampler(const ResourceGroup::Descriptor &desc) = 0;
        virtual SwapChain* CreateSwapChain(const SwapChain::Descriptor &desc) = 0;

        // layout object
        virtual ShaderFunction* CreateShaderFunction(const ShaderFunction::Descriptor &desc) = 0;
        virtual Shader* CreateShader(const Shader::Descriptor &desc) = 0;
        virtual GraphicsPipeline* CreatePipelineState(const GraphicsPipeline::Descriptor &desc) = 0;
        virtual ComputePipeline* CreatePipelineState(const ComputePipeline::Descriptor &desc) = 0;

        virtual PixelFormatFeatureFlags GetFormatFeatureFlags(PixelFormat format) const = 0;

        // command pool
        virtual CommandPool* CreateCommandPool(QueueType type) = 0;

        const DeviceCapability &GetCapability() const { return capability; }
        DeviceParallelPool* GetParallelContext() const { return threadPool.get(); }
    protected:
        virtual bool OnInit(const DeviceInit& init) = 0;
        virtual void UpdateDeviceCaps() = 0;
        virtual ThreadContext* CreateAsyncContext() = 0;

        DeviceCapability capability;

        std::unique_ptr<ThreadPool> threadPool;

        std::unique_ptr<ThreadContext> mainContext;
        std::vector<ThreadContext*> contexts;
    };

} // namespace sky::aurora
