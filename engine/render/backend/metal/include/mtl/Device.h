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
#include <mtl/Fence.h>
#include <mtl/Semaphore.h>
#include <mtl/VertexAssembly.h>
#include <mtl/QueryPool.h>
#include <mtl/DescriptorSetLayout.h>
#include <mtl/DescriptorSetPool.h>
#include <mtl/PipelineLayout.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLEvent.h>
#import <Metal/MTLCounters.h>

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
        MTLSharedEventListener *GetSharedEventListener() const { return sharedEventListener; }

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
        CREATE_DEV_OBJ(Fence)
        CREATE_DEV_OBJ(DescriptorSetLayout)
        CREATE_DEV_OBJ(PipelineLayout)
        CREATE_DEV_OBJ(DescriptorSetPool)
        CREATE_DEV_OBJ(QueryPool)
        CREATE_DEV_OBJ_FUNC(Semaphore, Sema)

        CREATE_DESC_OBJ(VertexInput)
        CREATE_DESC_OBJ(VertexAssembly)

        rhi::Queue* GetQueue(rhi::QueueType type) const override;
        id<MTLCounterSet> GetPipelineStatisticCounterSte() const { return pipelineStatisticCounterSet; }
        id<MTLCounterSet> GetTimeStampCounterSet() const { return timeStampCounterSet; }
        void WaitIdle() const override {}

    private:
        friend class Instance;
        bool Init(const Descriptor &);

        void UpdateCounterSet();
        id<MTLCounterSet> QueryCounterSet(MTLCommonCounterSet counterSet) const;
        id<MTLCounter> CheckCounterName(id<MTLCounterSet> set, MTLCommonCounter counter) const;
        uint32_t CheckPipelineStatisticFlags(const rhi::PipelineStatisticFlags &val, rhi::PipelineStatisticFlags &res) override;

        Device(Instance &);
        Instance    &instance;

        id<MTLDevice> device;
        id<MTLCounterSet> pipelineStatisticCounterSet = nil;
        id<MTLCounterSet> timeStampCounterSet = nil;
        rhi::PipelineStatisticFlags supportedPipelineStatistics;

        MTLSharedEventListener *sharedEventListener = nullptr;

        std::vector<QueuePtr> queues;
        Queue *graphicsQueue = nullptr;
        Queue *transferQueue = nullptr;
    };
}
