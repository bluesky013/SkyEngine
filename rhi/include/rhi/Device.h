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

#define CREATE_DEV_OBJ(name) \
    std::shared_ptr<rhi::name> Create##name(const rhi::name::Descriptor &desc) override \
    {                                                                                   \
        return std::static_pointer_cast<rhi::name>(CreateDeviceObject<name>(desc));     \
    }

namespace sky::rhi {

    class Device {
    public:
        Device() = default;
        virtual ~Device() = default;

        struct DeviceFeature {
            bool sparseBinding      = false;
            bool descriptorIndexing = false;
        };

        struct Descriptor {
            DeviceFeature feature;
        };

        virtual SwapChainPtr CreateSwapChain(const SwapChain::Descriptor &desc) = 0;
        virtual ImagePtr CreateImage(const Image::Descriptor &desc) = 0;
        virtual RenderPassPtr CreateRenderPass(const RenderPass::Descriptor &desc) = 0;
        virtual FrameBufferPtr CreateFrameBuffer(const FrameBuffer::Descriptor &desc) = 0;
        virtual CommandBufferPtr CreateCommandBuffer(const CommandBuffer::Descriptor &desc) = 0;
        virtual FencePtr CreateFence(const Fence::Descriptor &desc) = 0;

        virtual Queue* GetQueue(QueueType type) const = 0;

    protected:
        DeviceFeature enabledFeature;
    };

}
