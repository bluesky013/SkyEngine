//
// Created by Zach Lee on 2022/11/19.
//

#include <core/memory/TLSFAllocator.h>
#include <core/util/Memory.h>

namespace sky {

    void TLSFPool::Init(uint64_t size)
    {
        poolSize      = size == 0 ? POOL_SIZE : size;
        freeListCount = 0;

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

        uint64_t alignOffset = 0;
        if (freeListCount == 0) {
            if (!TryBlock(*nullBlock, size, alignment, alignOffset)) {
                return;
            }
            AllocateFromBlock(*nullBlock, alignOffset);
        }

        uint32_t fl = SizeToFLIndex(size);
        uint32_t sl = SizeToSLIndex(fl, size);
    }

    bool TLSFPool::TryBlock(const Block &block, uint64_t size, uint64_t alignment, uint64_t &alignOffset)
    {
        // buffer image granularity
        alignOffset = Align(block.offset, alignment);
        if (alignOffset + size > block.offset + block.size) {
            return false;
        }
        return true;
    }

    void TLSFPool::AllocateFromBlock(Block &current, uint64_t alignOffset)
    {
        if (&current != nullBlock) {
            RemoveFreeBlock(current);
        }

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
                uint32_t oldFl = SizeToFLIndex(prevPhy->size);
                uint32_t oldSl = SizeToSLIndex(oldFl, prevPhy->size);

                uint64_t newSize = prevPhy->size + padding;
                uint32_t newFl = SizeToFLIndex(newSize);
                uint32_t newSl = SizeToSLIndex(newFl, newSize);

                if (oldFl != newFl || oldSl != newSl) {
                    RemoveFreeBlock(*prevPhy, oldFl, oldSl);
                    prevPhy->size = newSize;
                    InsertFreeBlock(*prevPhy, newFl, newSl);
                }
            } else {
                Block &newBlock   = blocks.emplace_back();
                current.prevPhy   = &newBlock;
                prevPhy->nextPhy  = &newBlock;
                newBlock.prevPhy  = prevPhy;
                newBlock.nextPhy  = &current;
                newBlock.size     = padding;
                newBlock.offset   = current.offset;
                newBlock.nextFree = nullptr;
                newBlock.prevFree = nullptr;

                InsertFreeBlock(newBlock);
            }

            current.size -= padding;
            current.offset += padding;
        }

    }

    void TLSFPool::InsertFreeBlock(Block &)
    {

    }

    void TLSFPool::RemoveFreeBlock(Block &)
    {

    }

    void TLSFPool::InsertFreeBlock(Block &, uint32_t fl, uint32_t sl)
    {

    }

    void TLSFPool::RemoveFreeBlock(Block &, uint32_t fl, uint32_t sl)
    {

    }
}
