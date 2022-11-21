
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

        void Init(uint64_t size);

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
        static constexpr uint32_t MAX_FIRST_LEVEL_INDEX    = 63;
        static constexpr uint32_t SECOND_LEVEL_INDEX       = 4; // SLI, 4 or 5
        static constexpr uint32_t SECOND_LEVEL_INDEX_COUNT = 1 << SECOND_LEVEL_INDEX;
        static constexpr uint32_t MIN_BLOCK_SIZE           = 256; // MBS
        static constexpr uint32_t SMALL_BUFFER_STEP        = MIN_BLOCK_SIZE / SECOND_LEVEL_INDEX_COUNT;
        static constexpr uint32_t FIRST_LEVEL_OFFSET       = Log2(MIN_BLOCK_SIZE);
        static constexpr uint32_t POOL_SIZE                = 1024 * 1024 * 4;                          // 4M
        static constexpr uint32_t FIRST_LEVEL_INDEX        = Log2(POOL_SIZE) - FIRST_LEVEL_OFFSET + 1; // FLI

        struct Block {
            uint64_t offset = 0;
            uint64_t size   = 0;

            Block *prevPhy = nullptr;
            Block *nextPhy = nullptr;

            Block *prevFree = nullptr;
            Block *nextFree = nullptr;

            bool IsFree() const
            {
                return prevFree != this;
            }
        };

        void Allocate(uint64_t size, uint64_t alignment);

        static void                          LevelMapping(uint64_t size, uint32_t &fl, uint32_t &sl);
        static std::pair<uint32_t, uint32_t> LevelMapping(uint64_t);
        static std::pair<uint32_t, uint32_t> LevelMappingRoundUp(uint64_t);

    private:
        bool TryBlock(const Block &block, uint64_t size, uint64_t alignment, uint64_t &alignOffset);
        void AllocateFromBlock(Block &block, uint64_t size, uint64_t alignOffset);
        void AdjustAlignOffset(Block &block, uint64_t alignOffset);
        void UpdateBlock(Block &block, uint64_t size);
        void InsertFreeBlock(Block &);
        void RemoveFreeBlock(Block &);
        void InsertFreeBlock(Block &, uint32_t fl, uint32_t sl);
        void RemoveFreeBlock(Block &, uint32_t fl, uint32_t sl);

        static uint32_t SizeToFLIndex(uint64_t size)
        {
            return size > MIN_BLOCK_SIZE ? BitScanReverse(size) - FIRST_LEVEL_OFFSET + 1 : 0;
        }

        static uint32_t SizeToSLIndex(uint32_t fl, uint64_t size)
        {
            return (static_cast<uint32_t>(size >> (fl - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
        }

        Block           *nullBlock;
        uint64_t         poolSize;
        uint32_t         firstLevelIndex;
        uint32_t         flBitmap;
        uint32_t         slBitmaps[FIRST_LEVEL_INDEX];
        uint32_t         freeListCount;
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
