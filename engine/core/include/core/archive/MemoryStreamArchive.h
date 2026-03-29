//
// Created by blues on 2024/7/1.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <core/archive/BinaryData.h>
#include <streambuf>
#include <istream>
#include <ostream>
#include <vector>
#include <cstring>

namespace sky {

    /**
     * @brief A read-only streambuf backed by a raw memory region.
     *
     * Does NOT own the memory; the caller must ensure the buffer
     * outlives this object.
     */
    class MemoryInputStreamBuf : public std::streambuf {
    public:
        MemoryInputStreamBuf(const char *data, size_t size)
        {
            // const_cast is safe because we only read (setg)
            auto *p = const_cast<char *>(data);
            setg(p, p, p + size);
        }

    protected:
        pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                         std::ios_base::openmode /*which*/ = std::ios_base::in) override
        {
            char *base = eback();
            char *end  = egptr();

            switch (dir) {
            case std::ios_base::beg: break;
            case std::ios_base::cur: off += (gptr() - base); break;
            case std::ios_base::end: off += (end - base);    break;
            default: return pos_type(off_type(-1));
            }

            if (off < 0 || off > (end - base)) {
                return pos_type(off_type(-1));
            }
            setg(base, base + off, end);
            return pos_type(off);
        }

        pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override
        {
            return seekoff(off_type(pos), std::ios_base::beg, which);
        }
    };

    /**
     * @brief A write streambuf backed by a growable std::vector<char>.
     */
    class MemoryOutputStreamBuf : public std::streambuf {
    public:
        MemoryOutputStreamBuf() = default;

        const char *data() const { return buffer.data(); }
        size_t size() const { return buffer.size(); }

    protected:
        int_type overflow(int_type ch) override
        {
            if (ch != traits_type::eof()) {
                buffer.push_back(static_cast<char>(ch));
            }
            return ch;
        }

        std::streamsize xsputn(const char *s, std::streamsize count) override
        {
            buffer.insert(buffer.end(), s, s + count);
            return count;
        }

    private:
        std::vector<char> buffer;
    };

    /**
     * @brief A read-only IStreamArchive backed by raw memory.
     *
     * Wraps a MemoryInputStreamBuf + std::istream so it can be used
     * anywhere an IStreamArchive is expected (e.g. BinaryInputArchive).
     *
     * Accepts:
     *   - raw pointer + size
     *   - BinaryDataPtr (ref-counted, kept alive)
     */
    class IMemoryArchive : public IStreamArchive {
    public:
        /** Construct from raw pointer. Caller must keep the buffer alive. */
        IMemoryArchive(const char *data, size_t size)
            : buf(data, size)
            , istream(&buf)
            , IStreamArchive(istream)
        {
        }

        IMemoryArchive(const uint8_t *data, size_t size)
            : IMemoryArchive(reinterpret_cast<const char *>(data), size)
        {
        }

        /** Construct from BinaryData. Ref is held to keep memory alive. */
        explicit IMemoryArchive(const BinaryDataPtr &binaryData)
            : dataRef(binaryData)
            , buf(reinterpret_cast<const char *>(binaryData->Data()), binaryData->Size())
            , istream(&buf)
            , IStreamArchive(istream)
        {
        }

        ~IMemoryArchive() override = default;

    private:
        BinaryDataPtr dataRef;               // optional: keeps BinaryData alive
        MemoryInputStreamBuf buf;
        std::istream istream;
    };

    /**
     * @brief A write-only OStreamArchive backed by an in-memory buffer.
     *
     * After writing, call Data() / Size() to retrieve the result.
     */
    class OMemoryArchive : public OStreamArchive {
    public:
        OMemoryArchive()
            : buf()
            , ostream(&buf)
            , OStreamArchive(ostream)
        {
        }

        ~OMemoryArchive() override = default;

        const char *Data() const { return buf.data(); }
        size_t Size() const { return buf.size(); }

    private:
        MemoryOutputStreamBuf buf;
        std::ostream ostream;
    };

} // namespace sky
