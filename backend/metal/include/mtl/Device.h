//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <memory>
#include <rhi/Device.h>
#include <mtl/Swapchain.h>
#include <mtl/Buffer.h>
#include <mtl/Image.h>
#include <mtl/Queue.h>
#include <mtl/Sampler.h>
#include <mtl/GraphicsPipeline.h>
#include <mtl/RenderPass.h>
#include <mtl/Shader.h>
#include <mtl/VertexInput.h>
#include <mtl/FrameBuffer.h>
#include <mtl/CommandBuffer.h>
#include <Metal/MTLDevice.h>

namespace sky::mtl {
    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device();

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

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDescObject(const Desc &des)
        {
            auto res = std::make_shared<T>();
            if (!res->Init(des)) {
                res = nullptr;
            }
            return res;
        }

        id<MTLDevice> GetMetalDevice() const { return device; }

        // Device Object
        CREATE_DEV_OBJ(SwapChain)
        CREATE_DEV_OBJ(Image)
        CREATE_DEV_OBJ(Buffer)
        CREATE_DEV_OBJ(RenderPass)
        CREATE_DEV_OBJ(GraphicsPipeline)
        CREATE_DEV_OBJ(Sampler)
        CREATE_DEV_OBJ(Shader)
        CREATE_DEV_OBJ(FrameBuffer)
        CREATE_DEV_OBJ(CommandBuffer)

        CREATE_DESC_OBJ(VertexInput)

        rhi::FencePtr CreateFence(const rhi::Fence::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetLayoutPtr CreateDescriptorSetLayout(const rhi::DescriptorSetLayout::Descriptor &desc) override { return nullptr; }
        rhi::PipelineLayoutPtr CreatePipelineLayout(const rhi::PipelineLayout::Descriptor &desc) override { return nullptr; }
        rhi::SemaphorePtr CreateSema(const rhi::Semaphore::Descriptor &desc) override { return nullptr; }
        rhi::VertexAssemblyPtr CreateVertexAssembly(const rhi::VertexAssembly::Descriptor &desc) override { return nullptr; }
        rhi::DescriptorSetPoolPtr CreateDescriptorSetPool(const rhi::DescriptorSetPool::Descriptor &desc) override { return nullptr; }

        rhi::Queue* GetQueue(rhi::QueueType type) const override;

        void WaitIdle() const override {}

    private:
        friend class Instance;
        bool Init(const Descriptor &);

        Device(Instance &);
        Instance    &instance;

        id<MTLDevice> device;

        std::vector<QueuePtr> queues;
        Queue *graphicsQueue = nullptr;
        Queue *transferQueue = nullptr;
    };
}
