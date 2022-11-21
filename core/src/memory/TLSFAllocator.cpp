//
// Created by Zach Lee on 2022/11/19.
//

#include <core/memory/TLSFAllocator.h>
#include <core/util/Memory.h>

namespace sky {

    void TLSFPool::Init(uint64_t size)
    {
        poolSize        = size == 0 ? POOL_SIZE : size;
        freeListCount   = 0;

        nullBlock           = &blocks.emplace_back();
        nullBlock->offset   = 0;
        nullBlock->size     = poolSize;
        nullBlock->prevPhy  = nullptr;
        nullBlock->nextPhy  = nullptr;
        nullBlock->prevFree = nullptr;
        nullBlock->nextFree = nullptr;

        firstLevelIndex = static_cast<uint32_t>(Log2(poolSize)) - FIRST_LEVEL_OFFSET + 1U;
        flBitmap        = 0;
        for (uint32_t i = 0; i < FIRST_LEVEL_INDEX; ++i) {
            slBitmaps[i] = 0;
        }
    }

    void TLSFPool::Allocate(uint64_t size, uint64_t alignment)
    {
        uint64_t allocSize = Align(size, std::max(alignment, static_cast<uint64_t>(SMALL_BUFFER_STEP)));

        uint64_t alignOffset = 0;
        if (freeListCount == 0) {
            if (TryBlock(*nullBlock, allocSize, alignment, alignOffset)) {
                AllocateFromBlock(*nullBlock, allocSize, alignOffset);
                return;
            }
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
                Block &newBlock   = blocks.emplace_back();
                current.prevPhy   = &newBlock;
                prevPhy->nextPhy  = &newBlock;

                newBlock.size     = padding;
                newBlock.offset   = current.offset;
                newBlock.prevPhy  = prevPhy;
                newBlock.nextPhy  = &current;
                newBlock.nextFree = nullptr;
                newBlock.prevFree = nullptr;
                InsertFreeBlock(newBlock);
            }

            current.size -= padding;
            current.offset += padding;
        }
    }

    void TLSFPool::UpdateBlock(Block &current, uint64_t size)
    {
        uint64_t blockLeftSize = current.size - size;
        if (blockLeftSize == 0) {
            if (nullBlock == &current) {
                nullBlock           = &blocks.emplace_back();
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
            auto &newBlock    = blocks.emplace_back();
            newBlock.size     = blockLeftSize;
            newBlock.offset   = current.offset + size;
            newBlock.prevPhy  = &current;
            newBlock.nextPhy  = current.nextPhy;
            newBlock.prevFree = nullptr;
            newBlock.nextFree = nullptr;

            current.nextPhy = &newBlock;
            current.size    = size;
            if (nullBlock == &current) {
                nullBlock = &newBlock;
                current.prevPhy = &current;
            } else {
                newBlock.nextPhy->prevPhy = &newBlock;
                InsertFreeBlock(newBlock);
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
        if (size <= MIN_BLOCK_SIZE) {
            fl = 0;
            sl = static_cast<uint32_t>(size - 1) / SMALL_BUFFER_STEP;
        } else {
            fl = SizeToFLIndex(size);
            sl = SizeToSLIndex(fl, size);
        }
    }

    std::pair<uint32_t, uint32_t> TLSFPool::LevelMapping(uint64_t size)
    {
        uint32_t fl = 0;
        uint32_t sl = 0;
        LevelMapping(size, fl, sl);
        return {fl, sl};
    }

    std::pair<uint32_t, uint32_t> TLSFPool::LevelMappingRoundUp(uint64_t size)
    {
        uint64_t roundSize = size;
        if (size > MIN_BLOCK_SIZE) {
            roundSize += (1ULL << (BitScanReverse(size) - SECOND_LEVEL_INDEX));
        }
        return LevelMapping(roundSize);
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
