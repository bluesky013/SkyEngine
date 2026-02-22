//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace sky {

    class LinearStorage {
    public:
        explicit LinearStorage(size_t blockSize);
        ~LinearStorage() = default;

        LinearStorage(const LinearStorage &) = delete;
        LinearStorage &operator=(const LinearStorage &) = delete;
        LinearStorage(LinearStorage &&) noexcept = default;
        LinearStorage &operator=(LinearStorage &&) noexcept = default;

        uint8_t *Allocate(size_t size, size_t alignment = 1);
        void Reset();
        void Shrink();

        size_t GetBlockSize() const { return blockSize; }
        size_t GetBlockCount() const { return blocks.size(); }
        size_t GetTotalCapacity() const { return blocks.size() * blockSize; }
        size_t GetCurrentUsedSize() const;

    private:
        struct Block {
            std::unique_ptr<uint8_t[]> data;
            size_t offset = 0;
        };

        Block &CurrentBlock();
        void AddBlock();

        size_t blockSize;
        size_t currentIndex = 0;
        std::vector<Block> blocks;
    };

} // namespace sky