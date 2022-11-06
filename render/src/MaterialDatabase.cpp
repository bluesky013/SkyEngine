//
// Created by Zach Lee on 2022/8/17.
//

#include <render/RHIManager.h>
#include <render/MaterialDatabase.h>
#include <render/RenderConstants.h>

namespace sky {

    uint32_t MaterialDatabase::Level(uint32_t stride) const
    {
        uint32_t realStride = std::max(stride, MIN_MATERIAL_BUFFER_STRIDE);
        uint32_t level      = static_cast<uint32_t>(std::ceil(std::log2(realStride))) - MATERIAL_BUFFER_LEVEL_OFFSET;
        return level;
    }

    RDDynBufferViewPtr MaterialDatabase::Allocate(uint32_t stride)
    {
        uint32_t level = Level(stride);
        if (level >= pools.size()) {
            pools.resize(level + 1);
        }
        auto &pool = pools[level];
        if (!pool) {
            RenderBufferPool::Descriptor desc = {};
            desc.stride                       = 1 << (level + MATERIAL_BUFFER_LEVEL_OFFSET);
            desc.frame                        = frameNum;
            desc.count                        = std::min(MAX_MATERIAL_COUNT_PER_BLOCK, DEFAULT_MATERIAL_BUFFER_BLOCK / desc.stride);
            desc.usage                        = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            desc.memory                       = VMA_MEMORY_USAGE_CPU_TO_GPU;
            pool                              = std::make_unique<RenderBufferPool>(desc);
        }
        return pool->Allocate();
    }

} // namespace sky
