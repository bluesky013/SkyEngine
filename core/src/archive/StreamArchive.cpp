//
// Created by blues on 2023/10/11.
//

#include "core/archive/StreamArchive.h"

namespace sky {

    bool IStreamArchive::Load(char *data, uint32_t size)
    {
        return stream.rdbuf()->sgetn(data, size) == size;
    }

    bool OStreamArchive::Save(const char *data, uint32_t size)
    {
        return stream.rdbuf()->sputn(data, size) == size;
    }

} // namespace sky