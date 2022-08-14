//
// Created by Zach Lee on 2022/2/1.
//

#pragma once
#include <render/resources/Buffer.h>
#include <vector>

namespace sky {

    class RenderBufferPool {
    public:
        struct Descriptor {
            uint32_t            count     = 1;
            uint32_t            stride    = 4;
            uint32_t            frame     = 2;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
        };

        RenderBufferPool(Descriptor desc);
        ~RenderBufferPool();

        RDDynBufferViewPtr Allocate();

        void Free(RDDynBufferViewPtr view);

        void SwapBuffer();

    private:
        void AllocateBlock(uint32_t num);

        uint32_t currentFrame = 0;
        Descriptor descriptor;
        std::vector<RDBufferPtr> blocks;
        std::list<RDDynBufferViewPtr> active;
        std::list<RDDynBufferViewPtr> freeList;
    };

}
