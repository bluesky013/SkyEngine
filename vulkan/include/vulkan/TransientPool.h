//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <vulkan/MemoryPool.h>
#include <vulkan/Buffer.h>
#include <vulkan/Image.h>
#include <vk_mem_alloc.h>

namespace sky::vk {
    class Device;

    class TransientPool {
    public:
        ~TransientPool() = default;

        struct Descriptor {
            uint32_t blockSize = 16 * 1024 * 1024;
        };

        bool Init(const Descriptor &desc);

        void BeginFrame();
        void EndFrame();

        void InitBuffer(const BufferPtr &buffer);
        void ResetBuffer(const BufferPtr &buffer);

        void InitImage(const ImagePtr &image);
        void ResetImage(const ImagePtr &image);

    private:
        friend class Device;
        TransientPool(Device &);

        int32_t GetMemoryTypeIndex(Device &device);

        Device &device;
        VmaPool pool;
    };

}
