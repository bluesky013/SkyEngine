//
// Created by Zach Lee on 2022/1/13.
//

#pragma once

namespace sky {

    inline uint32_t Align(uint32_t size, uint32_t alignment)
    {
        return ((size + alignment - 1) & (~(alignment - 1)));
    }
}
