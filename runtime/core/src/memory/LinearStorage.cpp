//
// Created by Zach Lee on 2023/8/27.
//

#include <core/memory/LinearStorage.h>

namespace sky {

    LinearStorage::LinearStorage(uint32_t blockSize) : total(blockSize), offset(0)
    {
        storage = std::make_unique<uint8_t[]>(total);
        Reset();
    }

    uint8_t *LinearStorage::Allocate(uint32_t size)
    {
        if (offset + size > total) {
            return nullptr;
        }
        uint8_t *res = storage.get() + offset;
        offset += size;
        return res;
    }

    void LinearStorage::Reset()
    {
        offset = 0;
    }

} // namespace sky