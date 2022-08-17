//
// Created by Zach Lee on 2022/8/17.
//

#pragma once

#include <render/RenderBufferPool.h>

namespace sky {

    class MaterialDatabase {
    public:
        MaterialDatabase(uint32_t number = INFLIGHT_FRAME) : frameNum(std::max(1u, number)) {}
        ~MaterialDatabase() = default;

        RDDynBufferViewPtr Allocate(uint32_t stride);

        void Free(DynamicBufferView& view);

        uint32_t Level(uint32_t stride) const;

    private:
        using PoolPtr = std::unique_ptr<RenderBufferPool>;
        uint32_t frameNum;
        std::vector<PoolPtr> pools;
    };

}