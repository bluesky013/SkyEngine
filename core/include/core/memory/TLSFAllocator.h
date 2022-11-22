
//
// Created by Zach Lee on 2022/11/19.
//

#pragma once

#include <type_traits>
#include <list>
#include <vector>
#include <core/util/Memory.h>
#include <core/template/ObjectPool.h>

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
         * 2*24 : DEFAULT_POOL_SIZE(4M), FIRST_LEVEL_INDEX
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
        static constexpr uint32_t DEFAULT_POOL_SIZE        = 1024 * 1024 * 4; // 4M
        static constexpr uint32_t FIRST_LEVEL_INDEX        = Log2(DEFAULT_POOL_SIZE);
        static constexpr uint32_t FIRST_LEVEL_INDEX_COUNT  = FIRST_LEVEL_INDEX - FIRST_LEVEL_OFFSET + 1; // FLI
        static constexpr uint32_t DEFAULT_BLOCK_POOL_NUM   = 16;

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

        Block *Allocate(uint64_t size, uint64_t alignment);
        void   Free(Block *);

        void                          LevelMapping(uint64_t size, uint32_t &fl, uint32_t &sl);
        std::pair<uint32_t, uint32_t> LevelMapping(uint64_t);

        uint64_t RoundUpSize(uint64_t);
        Block   *SearchDefault(uint64_t size, uint64_t alignment, uint64_t &alignOffset);
        Block   *SearchFreeBlock(uint64_t size, uint32_t &fl, uint32_t &sl);

    private:
        bool TryBlock(const Block &block, uint64_t size, uint64_t alignment, uint64_t &alignOffset);
        void AllocateFromBlock(Block &block, uint64_t size, uint64_t alignOffset);
        void AdjustAlignOffset(Block &block, uint64_t alignOffset);
        void UpdateBlock(Block &block, uint64_t size);
        void MergePrevBlockToCurrent(Block &current, Block &prev);
        void InsertFreeBlock(Block &);
        void RemoveFreeBlock(Block &);
        void InsertFreeBlock(Block &, uint32_t fl, uint32_t sl);
        void RemoveFreeBlock(Block &, uint32_t fl, uint32_t sl);

        Block            *nullBlock;
        uint32_t          flBitmap;
        uint32_t          slBitmaps[FIRST_LEVEL_INDEX_COUNT];
        uint32_t          freeListCount;
        Block            *blockFreeList[FIRST_LEVEL_INDEX_COUNT][SECOND_LEVEL_INDEX_COUNT];
        ObjectPool<Block> blocks{DEFAULT_BLOCK_POOL_NUM};
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
