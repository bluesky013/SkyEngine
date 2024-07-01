//
// Created by blues on 2023/10/11.
//

#include "core/archive/StreamArchive.h"

namespace sky {

    bool IStreamArchive::LoadRaw(char *data, size_t size)
    {
        return stream.rdbuf()->sgetn(data, static_cast<std::streamsize>(size)) == size;
    }

    int IStreamArchive::Peek() const
    {
        return stream.peek();
    }

    int IStreamArchive::Get()
    {
        return stream.get();
    }

    size_t IStreamArchive::Tell() const
    {
        return static_cast<size_t>(stream.tellg());
    }

    bool OStreamArchive::SaveRaw(const char *data, size_t size)
    {
        return stream.rdbuf()->sputn(data, static_cast<std::streamsize>(size)) == size;
    }

    void OStreamArchive::Put(char ch)
    {
        stream.put(ch);
    }

    void OStreamArchive::Flush()
    {
        stream.flush();
    }
} // namespace sky