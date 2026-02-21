//
// Created by Zach Lee on 2023/8/27.
//

#include <core/memory/LinearStorage.h>

#include "core/util/Memory.h"

#include <cassert>

namespace sky {

    LinearStorage::LinearStorage(size_t blockSize_) : blockSize(blockSize_)
    {
        AddBlock();
    }

    LinearStorage::Block &LinearStorage::CurrentBlock()
    {
        return blocks[currentIndex];
    }

    void LinearStorage::AddBlock()
    {
        Block block;
        block.data = std::make_unique<uint8_t[]>(blockSize);
        block.offset = 0;
        blocks.emplace_back(std::move(block));
    }

    uint8_t *LinearStorage::Allocate(size_t size, size_t alignment)
    {
        assert(size <= blockSize && "Single allocation exceeds block size");
        size = Align(size, alignment);

        auto &blk = CurrentBlock();
        size_t aligned = Align(blk.offset, alignment);

        if (aligned + size <= blockSize) {
            uint8_t *ptr = blk.data.get() + aligned;
            blk.offset = aligned + size;
            return ptr;
        }

        // Current block exhausted, move to next or allocate new
        ++currentIndex;
        if (currentIndex >= blocks.size()) {
            AddBlock();
        }

        // New block starts at offset 0, which is always aligned for any power-of-two
        auto &newBlk = CurrentBlock();
        newBlk.offset = size;
        return newBlk.data.get();
    }

    void LinearStorage::Reset()
    {
        for (auto &blk : blocks) {
            blk.offset = 0;
        }
        currentIndex = 0;
    }

    void LinearStorage::Shrink()
    {
        if (blocks.size() > currentIndex + 1) {
            blocks.erase(blocks.begin() + static_cast<ptrdiff_t>(currentIndex) + 1, blocks.end());
        }
    }

    size_t LinearStorage::GetCurrentUsedSize() const
    {
        size_t total = 0;
        for (size_t i = 0; i < currentIndex; ++i) {
            total += blockSize;
        }
        if (currentIndex < blocks.size()) {
            total += blocks[currentIndex].offset;
        }
        return total;
    }

} // namespace sky