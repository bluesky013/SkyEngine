//
// Created by blues on 2023/10/11.
//

#include "core/archive/StreamArchive.h"

namespace sky {

    bool IStreamArchive::LoadRaw(char *data, size_t size)
    {
        return stream.rdbuf()->sgetn(data, static_cast<std::streamsize>(size)) == size;
    }

    bool OStreamArchive::SaveRaw(const char *data, size_t size)
    {
        return stream.rdbuf()->sputn(data, static_cast<std::streamsize>(size)) == size;
    }

} // namespace sky