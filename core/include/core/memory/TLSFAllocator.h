//
// Created by Zach Lee on 2022/11/19.
//

#pragma once

#include <type_traits>
#include <list>
#include <vector>
#include <core/util/Memory.h>

namespace sky {

    template <typename T>
    constexpr T Log2(T n)
    {
        return ((n < 2) ? 0 : 1 + Log2(n / 2));
    }

    class TLSFPool {
    public:
        TLSFPool()  = default;
        ~TLSFPool() = default;

        void Init();

        /**
         * 2*63 : MAX_FIRST_LEVEL_INDEX
         * ...
         * 2*24 : POOL_SIZE(4M), FIRST_LEVEL_INDEX
         * ...
         * 2^11
         * 2^10
         * 2^9
         * 2^8 : MIN_BLOCK_SIZE(256), FIRST_LEVEL_OFFSET(8)
         */
        static constexpr uint32_t MAX_FIRST_LEVEL_INDEX = 63;
        static constexpr uint32_t MIN_BLOCK_SIZE        = 256; // MBS
        static constexpr uint32_t FIRST_LEVEL_OFFSET    = Log2(MIN_BLOCK_SIZE);
        static constexpr uint32_t POOL_SIZE             = 1024 * 1024 * 4;                          // 4M
        static constexpr uint32_t FIRST_LEVEL_INDEX     = Log2(POOL_SIZE) - FIRST_LEVEL_OFFSET + 1; // FLI
        static constexpr uint32_t SECOND_LEVEL_INDEX    = 5;                                        // SLI, 4 or 5

        static uint32_t SizeToFLIndex(uint64_t size)
        {
            if (size > MIN_BLOCK_SIZE)
                return BitScanReverse(size) - FIRST_LEVEL_OFFSET;
            return 0;
        }

        static uint32_t SizeToSLIndex(uint32_t fl, uint64_t size)
        {
            return ((size >> (fl + FIRST_LEVEL_OFFSET - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
        }

        struct Block {
            uint64_t offset = 0;
            uint64_t size   = 0;

            Block *prevPhy = nullptr;
            Block *nextPhy = nullptr;

            Block *prevFree = nullptr;
            Block *nextFree = nullptr;
        };

    private:
        Block           *nullBlock;
        uint32_t         flBitmap;
        uint32_t         slBitmaps[FIRST_LEVEL_INDEX];
        Block           *blockFreeList[FIRST_LEVEL_INDEX][SECOND_LEVEL_INDEX];
        std::list<Block> blocks;
    };

    class TLSFAllocator {
    public:
        TLSFAllocator()  = default;
        ~TLSFAllocator() = default;

        struct Descriptor {
            uint64_t chunkSize;
        };
    };

}
