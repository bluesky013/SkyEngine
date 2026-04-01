//
// Created on 2026/04/02.
//

#include <MetalBuffer.h>
#include <MetalDevice.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalBuffer::MetalBuffer(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalBuffer::~MetalBuffer()
    {
        if (buffer != nullptr) {
            [(id<MTLBuffer>)buffer release];
            buffer = nullptr;
        }
    }

    bool MetalBuffer::Init(const Descriptor &desc)
    {
        if (desc.size == 0) {
            LOG_E(TAG, "buffer size must be greater than zero");
            return false;
        }

        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for buffer creation");
            return false;
        }

        auto options = ToMetalBufferOptions(desc.usage, desc.memory);
        auto *nativeBuffer = [metalDevice newBufferWithLength:desc.size options:options];
        if (nativeBuffer == nil) {
            LOG_E(TAG, "newBufferWithLength failed, size = %llu", desc.size);
            return false;
        }

        buffer = nativeBuffer;
        return true;
    }

} // namespace sky::aurora