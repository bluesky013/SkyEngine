//
// Created by blues on 2024/11/22.
//

#include "NameAllocator.h"
#include <core/platform/Platform.h>

namespace sky {

    NameStorageHandle NameAllocator::Allocate(uint16_t len)
    {
        if (!CheckCapacity(len)) {
            AllocateNewBlock();
        }

        SKY_ASSERT(!storages.empty());
        SKY_ASSERT(tail != nullptr);

        NameStorageHandle handle = {
            static_cast<uint16_t>(storages.size() - 1),
            tail->current
        };

        tail->current += len;
        return handle;
    }

    char *NameAllocator::Visit(const NameStorageHandle &handle)
    {
        return storages[handle.block].get() + handle.offset;
    }

    const char* NameAllocator::Visit(const NameStorageHandle &handle) const
    {
        return storages[handle.block].get() + handle.offset;
    }

    bool NameAllocator::CheckCapacity(uint16_t size) const
    {
        return tail != nullptr && (tail->current + size) <= BLOCK_SIZE;
    }

    void NameAllocator::AllocateNewBlock()
    {
        auto ptr = std::make_unique<char[]>(BLOCK_SIZE);
        memset(ptr.get(), 0, BLOCK_SIZE);

        tail = reinterpret_cast<NameStorageHeader*>(ptr.get());
        tail->current = static_cast<uint16_t>(sizeof(NameStorageHeader));

        storages.emplace_back(std::move(ptr));
    }

} // namespace sky