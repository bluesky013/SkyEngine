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
            uint32_t            blockSize = 0;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
            uint32_t            stride    = 0;
            uint32_t            frame     = 2;
        };

        RenderBufferPool(Descriptor desc);
        ~RenderBufferPool();

        void Reserve(uint32_t size);

        using BufferHandle = std::pair<drv::BufferPtr, uint32_t>;

        BufferHandle GetBuffer(uint32_t index) const;

        uint8_t* GetAddress(uint32_t index);

        void SwapBuffer();

    private:
        void AllocateBlock(uint32_t num);

        struct Block {
            drv::BufferPtr buffer;
            uint8_t* mappedPtr = nullptr;
        };

        uint32_t currentFrame;
        uint32_t validBlockSize;
        Descriptor descriptor;
        std::vector<Block> blocks;
    };

}
