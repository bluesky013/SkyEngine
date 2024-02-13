//
// Created by blues on 2024/1/14.
//

#include <core/archive/MemoryArchive.h>

namespace sky {

    bool MemoryArchive::Save(const char *data, size_t size)
    {
        auto offset = storage.size();
        storage.resize(offset + size);
        memcpy(&storage[offset], data, size);
        return true;
    }

} // namespace sky