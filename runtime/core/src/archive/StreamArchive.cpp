//
// Created by blues on 2023/10/11.
//

#include "core/archive/StreamArchive.h"

namespace sky {

    int StreamArchive::Peek()
    {
        return stream.peek();
    }

    int StreamArchive::Get()
    {
        return stream.get();
    }

    size_t StreamArchive::Tell() const
    {
        return stream.tellg();
    }

    void StreamArchive::Put(char ch)
    {
        stream.rdbuf()->sputc(ch);
    }

    bool StreamArchive::LoadRaw(char *data, size_t size)
    {
        return stream.rdbuf()->sgetn(data, static_cast<std::streamsize>(size)) == size;
    }

    bool StreamArchive::SaveRaw(const char *data, size_t size)
    {
        return stream.rdbuf()->sputn(data, static_cast<std::streamsize>(size)) == size;
    }

    bool IStreamArchive::LoadRaw(char *data, size_t size)
    {
        return stream.rdbuf()->sgetn(data, static_cast<std::streamsize>(size)) == size;
    }

    int IStreamArchive::Peek()
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