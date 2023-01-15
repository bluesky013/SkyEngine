//
// Created by Zach Lee on 2022/11/19.
//

#include <core/memory/TLSFAllocator.h>
#include <core/util/Memory.h>

namespace sky {

    void TLSFPool::Init()
    {
        freeListCount = 0;

        nullBlock           = blocks.Allocate();
        nullBlock->offset   = 0;
        nullBlock->size     = DEFAULT_POOL_SIZE;
        nullBlock->prevPhy  = nullptr;
        nullBlock->nextPhy  = nullptr;
        nullBlock->prevFree = nullptr;
        nullBlock->nextFree = nullptr;

        flBitmap = 0;
        for (uint32_t i = 0; i < FIRST_LEVEL_INDEX_COUNT; ++i) {
            slBitmaps[i] = 0;
            for (uint32_t j = 0; j < SECOND_LEVEL_INDEX_COUNT; ++j) {
                blockFreeList[i][j] = nullptr;
            }
        }
    }

    TLSFPool::Block* TLSFPool::Allocate(uint64_t size, uint64_t alignment)
    {
        uint64_t allocSize = Align(size, std::max(alignment, static_cast<uint64_t>(SMALL_BUFFER_STEP)));

        uint64_t alignOffset = 0;
        Block* block = SearchDefault(allocSize, alignment, alignOffset);
        if (block != nullptr) {
            AllocateFromBlock(*block, allocSize, alignOffset);
        }
        return block;
    }

    void TLSFPool::Free(Block *block)
    {
        if (block->prevPhy != nullptr && block->prevPhy->IsFree()) {
            // pre Phy block should be nullBlock
            RemoveFreeBlock(*block->prevPhy);
            MergePrevBlockToCurrent(*block, *block->prevPhy);
        }

        // block->nextPhy should not be nullptr
        if (!block->nextPhy->IsFree()) {
            InsertFreeBlock(*block);
        } else if (block->nextPhy == nullBlock){
            MergePrevBlockToCurrent(*block->nextPhy, *block);
        } else {
            Block *next = block->nextPhy;
            RemoveFreeBlock(*next);
            MergePrevBlockToCurrent(*next, *block);
            InsertFreeBlock(*next);
        }
    }

    bool TLSFPool::TryBlock(const Block &block, uint64_t size, uint64_t alignment, uint64_t &alignOffset)
    {
        // buffer image granularity
        alignOffset = Align(block.offset, alignment);
        return alignOffset + size <= block.offset + block.size;
    }

    void TLSFPool::AdjustAlignOffset(Block &current, uint64_t alignOffset)
    {
        uint64_t padding = alignOffset - current.offset;
        if (padding != 0) {
            // There should be no padding at the first allocation, prevPhy should not be nullptr;
            Block *prevPhy = current.prevPhy;

            /**
             * |----------prevPhy---------|-----------current---------|
             * |----------prevPhy---------|-padding-|-----current-----|
             *
             * block.size + padding may exceed the current block range.
             * if prevPhy is free, check if it needs to change freeList index.
             * if prevPhy is taken, add a padding block.
             */
            if (prevPhy->IsFree()) {
                uint64_t newSize = prevPhy->size + padding;

                auto [oldFl, oldSl] = LevelMapping(prevPhy->size);
                auto [newFl, newSl] = LevelMapping(newSize);

                if (oldFl != newFl || oldSl != newSl) {
                    RemoveFreeBlock(*prevPhy, oldFl, oldSl);
                    prevPhy->size = newSize;
                    InsertFreeBlock(*prevPhy, newFl, newSl);
                }
            } else {
                Block *newBlock  = blocks.Allocate();
                current.prevPhy  = newBlock;
                prevPhy->nextPhy = newBlock;

                newBlock->size     = padding;
                newBlock->offset   = current.offset;
                newBlock->prevPhy  = prevPhy;
                newBlock->nextPhy  = &current;
                newBlock->nextFree = nullptr;
                newBlock->prevFree = nullptr;
                InsertFreeBlock(*newBlock);
            }

            current.size -= padding;
            current.offset += padding;
        }
    }

    void TLSFPool::MergePrevBlockToCurrent(Block &current, Block &prev)
    {
        current.offset = prev.offset;
        current.size += prev.size;
        current.prevPhy = prev.prevPhy;
        if (current.prevPhy != nullptr) {
            current.prevPhy->nextPhy = &current;
        }
        blocks.Free(&prev);
    }

    void TLSFPool::UpdateBlock(Block &current, uint64_t size)
    {
        uint64_t blockLeftSize = current.size - size;
        if (blockLeftSize == 0) {
            if (nullBlock == &current) {
                nullBlock           = blocks.Allocate();
                nullBlock->size     = 0;
                nullBlock->offset   = current.offset + size;
                nullBlock->prevPhy  = &current;
                nullBlock->nextPhy  = nullptr;
                nullBlock->prevFree = nullptr;
                nullBlock->nextFree = nullptr;

                current.nextPhy  = nullBlock;
                current.prevFree = &current;
            }
        } else {
            auto *newBlock    = blocks.Allocate();
            newBlock->size     = blockLeftSize;
            newBlock->offset   = current.offset + size;
            newBlock->prevPhy  = &current;
            newBlock->nextPhy  = current.nextPhy;
            newBlock->prevFree = nullptr;
            newBlock->nextFree = nullptr;

            current.nextPhy = newBlock;
            current.size    = size;
            if (nullBlock == &current) {
                nullBlock = newBlock;
                current.prevFree = &current;
            } else {
                newBlock->nextPhy->prevPhy = newBlock;
                InsertFreeBlock(*newBlock);
            }
        }
    }

    void TLSFPool::AllocateFromBlock(Block &current, uint64_t size, uint64_t alignOffset)
    {
        if (&current != nullBlock) {
            RemoveFreeBlock(current);
        }

        AdjustAlignOffset(current, alignOffset);
        UpdateBlock(current, size);
    }

    void TLSFPool::InsertFreeBlock(Block &block)
    {
        auto [fl, sl] = LevelMapping(block.size);
        InsertFreeBlock(block, fl, sl);
    }

    void TLSFPool::RemoveFreeBlock(Block &block)
    {
        auto [fl, sl] = LevelMapping(block.size);
        RemoveFreeBlock(block, fl, sl);
    }

    void TLSFPool::LevelMapping(uint64_t size, uint32_t &fl, uint32_t &sl)
    {
        if (size < MIN_BLOCK_SIZE) {
            fl = 0;
            sl = static_cast<uint32_t>(size) / SMALL_BUFFER_STEP;
        } else {
            fl = BitScanReverse(size);
            sl = static_cast<uint32_t>(size >> (fl - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX);
            fl -= (FIRST_LEVEL_OFFSET - 1);
        }
    }

    std::pair<uint32_t, uint32_t> TLSFPool::LevelMapping(uint64_t size)
    {
        uint32_t fl = 0;
        uint32_t sl = 0;
        LevelMapping(size, fl, sl);
        return {fl, sl};
    }

    uint64_t TLSFPool::RoundUpSize(uint64_t size)
    {
        uint64_t roundSize = size;
        if (size >= MIN_BLOCK_SIZE) {
            roundSize += (1ULL << (BitScanReverse(size) - SECOND_LEVEL_INDEX)) - 1;
        } else {
            roundSize += SMALL_BUFFER_STEP - 1;
        }
        return roundSize;
    }

    TLSFPool::Block* TLSFPool::SearchDefault(uint64_t size, uint64_t alignment, uint64_t &alignOffset)
    {
        // quick check free list
        if (freeListCount == 0) {
            return TryBlock(*nullBlock, size, alignment, alignOffset) ? nullBlock : nullptr;
        }

        // try next level
        {
            uint64_t roundupSize = RoundUpSize(size);
            auto [fl, sl]        = LevelMapping(roundupSize);
            auto *freeBlock = SearchFreeBlock(roundupSize, fl, sl);
            while (freeBlock != nullptr) {
                if (TryBlock(*freeBlock, size, alignment, alignOffset)) {
                    return freeBlock;
                }
                freeBlock = freeBlock->nextFree;
            }
        }

        // try null block again
        if (TryBlock(*nullBlock, size, alignment, alignOffset)) {
            return nullBlock;
        }

        // try best fit
        {
            auto [fl, sl]        = LevelMapping(size);
            auto *freeBlock = SearchFreeBlock(size, fl, sl);
            while (freeBlock != nullptr) {
                if (TryBlock(*freeBlock, size, alignment, alignOffset)) {
                    return freeBlock;
                }
                freeBlock = freeBlock->nextFree;
            }
        }
        return nullptr;
    }

    TLSFPool::Block *TLSFPool::SearchFreeBlock(uint64_t size, uint32_t &fl, uint32_t &sl)
    {
        uint32_t slMap = slBitmaps[fl] & (~(0U) << sl);
        if (slMap == 0) {
            uint32_t flMap = flBitmap & (~(0U) << (fl + 1));
            if (flMap == 0) {
                // no available free blocks
                return nullptr;
            }

            fl = BitScan(flMap);
            slMap = slBitmaps[fl];
        }
        sl = BitScan(slMap);
        return blockFreeList[fl][sl];
    }

    void TLSFPool::InsertFreeBlock(Block &block, uint32_t fl, uint32_t sl)
    {
        /**
         * blockFreeList[i][j] --next--> [free1] --next--> [free2]
         * |
         * insert new block here
         */
        block.prevFree = nullptr;
        block.nextFree = blockFreeList[fl][sl];
        if (block.nextFree != nullptr) {
            block.nextFree->prevFree = &block;
        } else {
            // set bitmap
            flBitmap |= (1U << fl);
            slBitmaps[fl] |= (1U << sl);
        }
        blockFreeList[fl][sl] = &block;
        ++freeListCount;
    }

    void TLSFPool::RemoveFreeBlock(Block &block, uint32_t fl, uint32_t sl)
    {
        /**
         * 1. remove first block
         * blockFreeList[i][j] --next--> [free1] --next--> [free2]
         *        |
         *        remove this block
         *
         * 2. remove middle block
         * blockFreeList[i][j] --next--> [free1] --next--> [free2]
         *                                  |
         *                                  remove this block
         */

        // update tail
        if (block.nextFree != nullptr) {
            block.nextFree->prevFree = block.prevFree;
        }

        // update head
        if (block.prevFree != nullptr) {
            block.prevFree->nextFree = block.nextFree;
        } else {
            blockFreeList[fl][sl] = block.nextFree;

            // update bitmap
            if (block.nextFree == nullptr) {
                slBitmaps[fl] &= ~(1U << sl);
                if (slBitmaps[fl] == 0) {
                    flBitmap &= ~(1U << fl);
                }
            }
        }
        block.prevFree = &block;
        block.nextFree = nullptr;
        --freeListCount;
    }
}
