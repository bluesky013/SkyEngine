//
// Created by blues on 2024/4/6.
//

#pragma once

#include <list>
#include <memory>
#include <core/util/Memory.h>

namespace sky {

    template <typename T>
    class LinkedStorage {
    public:
        explicit LinkedStorage(uint32_t blockSize_) : blockSize(blockSize_) {}
        ~LinkedStorage() = default;

        uint8_t *Allocate(uint32_t size, uint32_t alignment)
        {
            if (storages.empty()) {
                NewStorage();
                return AllocateImpl(size, alignment);
            }

            uint8_t *ptr = AllocateImpl(size, alignment);
            if (ptr == nullptr) {
                NewStorage();
                ptr = AllocateImpl(size, alignment);
            }
            return ptr;
        }

        void NewStorage()
        {
            storages.emplace_back(std::make_unique<T>(blockSize));
        }

    private:
        uint8_t *AllocateImpl(uint32_t size, uint32_t alignment)
        {
            auto &storage = storages.back();
            return storage->Allocate(Align(size, alignment));
        }

        using StoragePtr = std::unique_ptr<T>;
        std::list<StoragePtr> storages;

        uint32_t blockSize = 16;
    };

} // namespace sky