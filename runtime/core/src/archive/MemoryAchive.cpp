//
// Created by blues on 2024/1/14.
//

#include <core/archive/MemoryArchive.h>

namespace sky {

    bool MemoryArchive::LoadRaw(char *val, size_t size)
    {
        if (pointer + size > data.size()) {
            return false;
        }

        memcpy(val, data.data() + pointer, size);
        pointer += size;
        return true;
    }

    bool MemoryArchive::SaveRaw(const char *val, size_t size)
    {
        if (pointer + size > data.size()) {
            data.resize(pointer + size);
        }

        memcpy(data.data() + pointer, val, size);
        pointer += size;
        return true;
    }

    const char* MemoryArchive::Data() const
    {
        return data.data();
    }

    char* MemoryArchive::Address()
    {
        return data.data();
    }

    size_t MemoryArchive::Size() const
    {
        return data.size();
    }

    void MemoryArchive::Resize(size_t size)
    {
        data.resize(size);
    }

    void MemoryArchive::Seek(size_t offset)
    {
        pointer = offset;
    }

} // namespace sky
