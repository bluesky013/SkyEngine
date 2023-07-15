//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <memory>
#include <rhi/Swapchain.h>
#include <rhi/Image.h>
#include <rhi/RenderPass.h>
#include <rhi/FrameBuffer.h>
#include <rhi/CommandBuffer.h>
#include <rhi/Fence.h>
#include <rhi/Shader.h>
#include <rhi/GraphicsPipeline.h>
#include <rhi/Semaphore.h>
#include <rhi/DescriptorSet.h>
#include <rhi/VertexAssembly.h>
#include <rhi/DescriptorSetPool.h>
#include <rhi/QueryPool.h>

#define CREATE_DEV_OBJ(name) \
    std::shared_ptr<rhi::name> Create##name(const rhi::name::Descriptor &desc) override \
    {                                                                                   \
        return std::static_pointer_cast<rhi::name>(CreateDeviceObject<name>(desc));     \
    }

#define CREATE_DEV_OBJ_FUNC(name, func) \
    std::shared_ptr<rhi::name> Create##func(const rhi::name::Descriptor &desc) override \
    {                                                                                   \
        return std::static_pointer_cast<rhi::name>(CreateDeviceObject<name>(desc));     \
    }

#define CREATE_DESC_OBJ(name) \
    std::shared_ptr<rhi::name> Create##name(const rhi::name::Descriptor &desc) override \
    {                                                                                   \
        return std::static_pointer_cast<rhi::name>(CreateDescObject<name>(desc));       \
    }

namespace sky::rhi {

    class Device {
    public:
        Device() = default;
        virtual ~Device() = default;

        struct DeviceFeature {
            bool sparseBinding = false;
            bool descriptorIndexing = false;
            bool variableRateShading = false;
            bool multiView = false;
            bool framebufferFetch = false;
            bool frameBufferFetchDS = false;
            bool frameBufferFetchNoCoherent = false;
            bool multiDrawIndirect = false;
            bool firstInstanceIndirect = false;
            bool depthStencilResolve = false;
        };

        struct Limitation {
            uint32_t maxColorAttachments = 1;
            uint32_t maxDrawBuffers = 1;
            uint32_t maxDrawIndirectCount = 1;
        };

        struct Descriptor {
            DeviceFeature feature;
        };
        virtual void WaitIdle() const = 0;

        // device object
        virtual SwapChainPtr CreateSwapChain(const SwapChain::Descriptor &desc) = 0;
        virtual BufferPtr CreateBuffer(const Buffer::Descriptor &desc) = 0;
        virtual ImagePtr CreateImage(const Image::Descriptor &desc) = 0;
        virtual RenderPassPtr CreateRenderPass(const RenderPass::Descriptor &desc) = 0;
        virtual FrameBufferPtr CreateFrameBuffer(const FrameBuffer::Descriptor &desc) = 0;
        virtual CommandBufferPtr CreateCommandBuffer(const CommandBuffer::Descriptor &desc) = 0;
        virtual FencePtr CreateFence(const Fence::Descriptor &desc) = 0;
        virtual ShaderPtr CreateShader(const Shader::Descriptor &desc) = 0;
        virtual GraphicsPipelinePtr CreateGraphicsPipeline(const GraphicsPipeline::Descriptor &desc) = 0;
        virtual DescriptorSetLayoutPtr CreateDescriptorSetLayout(const DescriptorSetLayout::Descriptor &desc) = 0;
        virtual PipelineLayoutPtr CreatePipelineLayout(const PipelineLayout::Descriptor &desc) = 0;
        virtual SemaphorePtr CreateSema(const Semaphore::Descriptor &desc) = 0;
        virtual VertexAssemblyPtr CreateVertexAssembly(const VertexAssembly::Descriptor &desc) = 0;
        virtual SamplerPtr CreateSampler(const Sampler::Descriptor &desc) = 0;
        virtual DescriptorSetPoolPtr CreateDescriptorSetPool(const DescriptorSetPool::Descriptor &desc) = 0;
        virtual QueryPoolPtr CreateQueryPool(const QueryPool::Descriptor &desc) = 0;

        // query
        virtual uint32_t CheckPipelineStatisticFlags(const PipelineStatisticFlags &val, PipelineStatisticFlags &res) { return 0; }

        // layout object
        virtual VertexInputPtr CreateVertexInput(const VertexInput::Descriptor &desc) = 0;

        virtual Queue* GetQueue(QueueType type) const = 0;

        const DeviceFeature &GetFeatures() const { return enabledFeature; }
        const Limitation &GetLimitations() const { return limitation; }

    protected:
        DeviceFeature enabledFeature;
        Limitation limitation;
    };

}
