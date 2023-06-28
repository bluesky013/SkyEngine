//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <RHI/Device.h>

#include <dx12/Base.h>
#include <dx12/Queue.h>

namespace sky::dx {
    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device() override = default;

        template <typename T>
        inline std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

        ID3D12Device *GetDevice() const;
        IDXGIFactory2 *GetDXGIFactory() const;
        Queue *GetGraphicsQueue() const;

        void WaitIdle() const {}
        rhi::Queue* GetQueue(rhi::QueueType type) const override { return nullptr; }
        // device object
        rhi::SwapChainPtr CreateSwapChain(const rhi::SwapChain::Descriptor &desc) override { return nullptr; }
        rhi::ImagePtr CreateImage(const rhi::Image::Descriptor &desc) override { return nullptr; }
        rhi::BufferPtr CreateBuffer(const rhi::Buffer::Descriptor &desc) override { return nullptr; }
        rhi::RenderPassPtr CreateRenderPass(const rhi::RenderPass::Descriptor &desc) override { return nullptr; }
        rhi::FrameBufferPtr CreateFrameBuffer(const rhi::FrameBuffer::Descriptor &desc) override { return nullptr; }
        rhi::CommandBufferPtr CreateCommandBuffer(const CommandBuffer::Descriptor &desc) override { return nullptr; }
        rhi::FencePtr CreateFence(const rhi::Fence::Descriptor &desc) override { return nullptr; }
        rhi::ShaderPtr CreateShader(const rhi::Shader::Descriptor &desc) override { return nullptr; }
        rhi::GraphicsPipelinePtr CreateGraphicsPipeline(const rhi::GraphicsPipeline::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetLayoutPtr CreateDescriptorSetLayout(const rhi::DescriptorSetLayout::Descriptor &desc) override { return nullptr; }
        rhi::PipelineLayoutPtr CreatePipelineLayout(const rhi::PipelineLayout::Descriptor &desc) override { return nullptr; }
        rhi::SemaphorePtr CreateSema(const rhi::Semaphore::Descriptor &desc) override { return nullptr; }
        rhi::VertexInputPtr CreateVertexInput(const rhi::VertexInput::Descriptor &desc) override { return nullptr; }
        rhi::VertexAssemblyPtr CreateVertexAssembly(const rhi::VertexAssembly::Descriptor &desc) override { return nullptr; }
        rhi::SamplerPtr CreateSampler(const rhi::Sampler::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetPoolPtr CreateDescriptorSetPool(const rhi::DescriptorSetPool::Descriptor &desc) override { return nullptr; }
    private:
        friend class Instance;
        bool Init(const Descriptor &, ComPtr<IDXGIAdapter1> &adaptor);

        Device(Instance &);
        Instance &instance;

        ComPtr<ID3D12Device> device;
        QueuePtr graphicsQueue;
    };

}