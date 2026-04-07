//
// Created on 2026/04/07.
//

#import <Metal/Metal.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>
#include <MetalEncoder.h>

namespace sky::aurora {

    // ---- MetalCommandBuffer ----

    MetalCommandBuffer::MetalCommandBuffer(MetalDevice &device)
        : device(device)
    {
    }

    MetalCommandBuffer::~MetalCommandBuffer()
    {
        if (cmdBuffer != nullptr) {
            id<MTLCommandBuffer> cb = (__bridge_transfer id<MTLCommandBuffer>)cmdBuffer;
            cb = nil;
            cmdBuffer = nullptr;
        }
    }

    void MetalCommandBuffer::Begin()
    {
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)device.GetCommandQueue();
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        cmdBuffer = (__bridge_retained void *)cb;
    }

    void MetalCommandBuffer::End()
    {
        // Metal command buffer commit happens at submit time, not here.
    }

    std::unique_ptr<GraphicsEncoder> MetalCommandBuffer::CreateGraphicsEncoder()
    {
        return std::make_unique<MetalGraphicsEncoder>(device, cmdBuffer);
    }

    std::unique_ptr<ComputeEncoder> MetalCommandBuffer::CreateComputeEncoder()
    {
        return std::make_unique<MetalComputeEncoder>(device, cmdBuffer);
    }

    std::unique_ptr<BlitEncoder> MetalCommandBuffer::CreateBlitEncoder()
    {
        return std::make_unique<MetalBlitEncoder>(device, cmdBuffer);
    }

    // ---- MetalCommandPool ----

    MetalCommandPool::MetalCommandPool(MetalDevice &device)
        : device(device)
    {
    }

    MetalCommandPool::~MetalCommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();
    }

    bool MetalCommandPool::Init()
    {
        return true;
    }

    void MetalCommandPool::Reset()
    {
    }

    CommandBuffer *MetalCommandPool::Allocate()
    {
        auto *cmdBuffer = new MetalCommandBuffer(device);
        allocatedBuffers.push_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora
