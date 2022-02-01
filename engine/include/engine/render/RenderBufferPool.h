//
// Created by Zach Lee on 2022/2/1.
//

#pragma once
#include <vulkan/Buffer.h>
#include <vector>

namespace sky {

    class RenderBufferPool {
    public:
        struct Descriptor {
            VkDeviceSize        blockSize = 0;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
        };

        RenderBufferPool(Descriptor desc) : descriptor(std::move(desc)) {}
        ~RenderBufferPool() = default;

        void Init();

        struct Block {
            drv::BufferPtr shadow;
            drv::BufferPtr buffer;
            uint32_t offset;
        };
    private:
        Descriptor descriptor;
        std::vector<Block> blocks;
    };

}
