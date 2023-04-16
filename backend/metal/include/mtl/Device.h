//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <memory>
#include <rhi/Device.h>
#include <mtl/Swapchain.h>
#include <Metal/Metal.hpp>

namespace sky::mtl {
    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device() = default;

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

        MTL::Device *GetMetalDevice() const;

        // Device Object
        CREATE_DEV_OBJ(SwapChain)
        rhi::ImagePtr CreateImage(const rhi::Image::Descriptor &desc) override { return nullptr; }
        rhi::BufferPtr CreateBuffer(const rhi::Buffer::Descriptor &desc) override { return nullptr; }
        rhi::RenderPassPtr CreateRenderPass(const rhi::RenderPass::Descriptor &desc) override { return nullptr; }
        rhi::FrameBufferPtr CreateFrameBuffer(const rhi::FrameBuffer::Descriptor &desc) override { return nullptr; }
        rhi::CommandBufferPtr CreateCommandBuffer(const rhi::CommandBuffer::Descriptor &desc) override { return nullptr; }
        rhi::FencePtr CreateFence(const rhi::Fence::Descriptor &desc) override { return nullptr; }
        rhi::ShaderPtr CreateShader(const rhi::Shader::Descriptor &desc) override { return nullptr; }
        rhi::GraphicsPipelinePtr CreateGraphicsPipeline(const rhi::GraphicsPipeline::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetLayoutPtr CreateDescriptorSetLayout(const rhi::DescriptorSetLayout::Descriptor &desc) override { return nullptr; }
        rhi::PipelineLayoutPtr CreatePipelineLayout(const rhi::PipelineLayout::Descriptor &desc) override { return nullptr; }
        rhi::SemaphorePtr CreateSema(const rhi::Semaphore::Descriptor &desc) override { return nullptr; }
        rhi::VertexInputPtr CreateVertexInput(const rhi::VertexInput::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetPtr CreateDescriptorSet(const rhi::DescriptorSet::Descriptor &desc) override { return nullptr; }
        rhi::VertexAssemblyPtr CreateVertexAssembly(const rhi::VertexAssembly::Descriptor &desc) override { return nullptr; }

        rhi::Queue* GetQueue(rhi::QueueType type) const override { return nullptr; }

    private:
        friend class Instance;
        bool Init(const Descriptor &);

        Device(Instance &);
        Instance    &instance;
        MTL::Device *device = nullptr;
    };
}
