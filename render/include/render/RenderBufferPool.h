//
// Created by Zach Lee on 2022/2/1.
//

#pragma once
#include <render/resources/Buffer.h>
#include <render/RenderConstants.h>
#include <vector>

namespace sky {

    class RenderBufferPool {
    public:
        struct Descriptor {
            uint32_t            count     = 1;
            uint32_t            stride    = 4;
            uint32_t            frame     = INFLIGHT_FRAME;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
        };

        RenderBufferPool(Descriptor desc);
        ~RenderBufferPool();

        RDDynBufferViewPtr Allocate();

        void Free(RDDynBufferViewPtr view);

        void Update();

    private:
        void AllocateBlock(uint32_t num);

        Descriptor descriptor;
        std::vector<RDBufferPtr> blocks;
        std::list<RDDynBufferViewPtr> active;
        std::list<RDDynBufferViewPtr> freeList;
    };

}
