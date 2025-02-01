//
// Created by blues on 2024/11/22.
//

#pragma once

#include <core/name/NameTypes.h>

#include <vector>
#include <memory>

namespace sky {


    class NameAllocator {
    public:
        NameAllocator() = default;
        ~NameAllocator() = default;

        static constexpr uint32_t BLOCK_SIZE = 65536;
        static constexpr uint32_t MAX_NAME_LEN = BLOCK_SIZE - sizeof(NameStorageHeader);

        using Storage = std::unique_ptr<char[]>;

        NameStorageHandle Allocate(uint16_t len);

        char *Visit(const NameStorageHandle &handle);
        const char* Visit(const NameStorageHandle &handle) const;

    private:
        bool CheckCapacity(uint16_t size) const;
        void AllocateNewBlock();

        std::vector<Storage> storages;
        NameStorageHeader* tail = nullptr;
    };

} // namespace sky