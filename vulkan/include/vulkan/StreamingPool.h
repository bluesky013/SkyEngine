//
// Created by Zach Lee on 2022/10/19.
//

#pragma once

#include <vulkan/Buffer.h>
#include <vulkan/Image.h>

namespace sky::drv {
    class Device;

    class StreamingPool {
    public:
        explicit StreamingPool(Device &dev) : device(dev) {}
        ~StreamingPool() = default;

        struct Descriptor {
            uint32_t blockSize = 32 * 1024 * 1024; // 32M
        };
        void Setup(const Descriptor &desc);

        struct BufferPackage {
            uint8_t *data = nullptr;
            uint32_t size = 0;
        };

        struct ImagePackage {
            uint8_t *data = nullptr;
            uint32_t size = 0;
            uint32_t layer = 0;
            uint32_t level = 0;
        };

        void UploadBuffer(const BufferPtr &buffer, const BufferPackage &package);
        void UploadImage(const ImagePtr &image, const ImagePackage &package);

    private:
        Device &device;
        Descriptor info;
        BufferPtr stagingBufferPool;
    };

}