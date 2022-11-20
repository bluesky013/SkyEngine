//
// Created by Zach Lee on 2022/11/19.
//

#include <core/memory/TLSFAllocator.h>

namespace sky {

    void TLSFPool::Init()
    {
        nullBlock = nullptr;
        flBitmap = 0;

        for (uint32_t i = 0; i < FIRST_LEVEL_INDEX; ++i) {
            slBitmaps[i] = 0;
            for (uint32_t j = 0; j < SECOND_LEVEL_INDEX; ++j) {
                blockFreeList[i][j] = nullptr;
            }
        }
    }

}
