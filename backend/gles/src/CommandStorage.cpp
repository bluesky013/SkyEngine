//
// Created by Zach on 2023/1/31.
//

#include <gles/CommandStorage.h>

namespace sky::gles {

    void CommandStorage::Init(const Descriptor &desc)
    {
        total = desc.blockSize;
        storage = std::make_unique<uint8_t[]>(total);
        Reset();
    }

    uint8_t *CommandStorage::Allocate(uint32_t size)
    {
        if (offset + size > total) {
            return nullptr;
        }
        uint8_t *res = storage.get() + offset;
        offset += size;
        return res;
    }

    void CommandStorage::Reset()
    {
        offset = 0;
    }

}
