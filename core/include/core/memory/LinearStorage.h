//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <cstdint>
#include <memory>

namespace sky {

    class LinearStorage {
    public:
        explicit LinearStorage(uint32_t blockSize);
        ~LinearStorage() = default;

        uint8_t *Allocate(uint32_t size);
        void Reset();

    private:
        uint32_t total;
        uint32_t offset;
        std::unique_ptr<uint8_t[]> storage;
    };

} // namespace sky