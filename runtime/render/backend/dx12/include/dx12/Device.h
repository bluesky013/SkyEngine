//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <RHI/Device.h>

#include <dx12/Base.h>
#include <dx12/Queue.h>
#include <dx12/Shader.h>
#include <dx12/GraphicsPipeline.h>
#include <dx12/DescriptorSetPool.h>
#include <dx12/DescriptorSetLayout.h>
#include <dx12/PipelineLayout.h>

namespace sky::dx {
    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device() override;

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

        ID3D12Device1 *GetDevice() const;
        IDXGIFactory2 *GetDXGIFactory() const;

        void WaitIdle() const override {}
        rhi::Queue* GetQueue(rhi::QueueType type) const override;
        // device object
        CREATE_DEV_OBJ(Shader)
        CREATE_DEV_OBJ(GraphicsPipeline)
        CREATE_DEV_OBJ(DescriptorSetPool)
        CREATE_DEV_OBJ(DescriptorSetLayout)
        CREATE_DEV_OBJ(PipelineLayout)

        rhi::SwapChainPtr CreateSwapChain(const rhi::SwapChain::Descriptor &desc) override { return nullptr; }
        rhi::ImagePtr CreateImage(const rhi::Image::Descriptor &desc) override { return nullptr; }
        rhi::BufferPtr CreateBuffer(const rhi::Buffer::Descriptor &desc) override { return nullptr; }
        rhi::RenderPassPtr CreateRenderPass(const rhi::RenderPass::Descriptor &desc) override { return nullptr; }
        rhi::FrameBufferPtr CreateFrameBuffer(const rhi::FrameBuffer::Descriptor &desc) override { return nullptr; }
        rhi::CommandBufferPtr CreateCommandBuffer(const CommandBuffer::Descriptor &desc) override { return nullptr; }
        rhi::FencePtr CreateFence(const rhi::Fence::Descriptor &desc) override { return nullptr; }
        rhi::SemaphorePtr CreateSema(const rhi::Semaphore::Descriptor &desc) override { return nullptr; }
        rhi::VertexInputPtr CreateVertexInput(const rhi::VertexInput::Descriptor &desc) override { return nullptr; }
        rhi::VertexAssemblyPtr CreateVertexAssembly(const rhi::VertexAssembly::Descriptor &desc) override { return nullptr; }
        rhi::SamplerPtr CreateSampler(const rhi::Sampler::Descriptor &desc) override { return nullptr; }
        rhi::QueryPoolPtr CreateQueryPool(const rhi::QueryPool::Descriptor &desc) override { return nullptr; }
    private:
        friend class Instance;
        bool Init(const Descriptor &, ComPtr<IDXGIAdapter1> &adaptor);

        explicit Device(Instance &);
        Instance &instance;

        ComPtr<ID3D12Device1> device;
        std::vector<QueuePtr> queues;
        Queue *graphicsQueue;
        Queue *computeQueue;
        Queue *transferQueue;
    };

}