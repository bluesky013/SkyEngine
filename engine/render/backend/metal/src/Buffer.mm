//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Buffer.h>
#include <mtl/Device.h>
#include <mtl/Conversion.h>

namespace sky::mtl {
    Buffer::~Buffer()
    {
        if (buffer) {
            [buffer release];
        }
    }

    bool Buffer::Init(const Descriptor &desc)
    {
        bufferDesc = desc;
        auto *mtlDevice = device.GetMetalDevice();
        const auto storageMode = FromRHI(desc.usage, desc.memory);
        // MTLResourceCPUCacheModeWriteCombined might be used for transfer
        const auto cacheMode = MTLResourceCPUCacheModeDefaultCache;
        const auto trackingMode = MTLResourceHazardTrackingModeTracked;

        MTLResourceOptions resourceOptions = storageMode | cacheMode | trackingMode;

        buffer = [device.GetMetalDevice() newBufferWithLength: desc.size
                                                      options: resourceOptions];
        return true;
    }

    uint8_t *Buffer::Map()
    {
        return static_cast<uint8_t *>(buffer.contents);
    }

    void Buffer::UnMap()
    {
    }
}
