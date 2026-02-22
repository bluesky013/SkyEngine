//
// Created by Zach Lee on 2023/9/12.
//

#include <rhi/Stream.h>

namespace sky::rhi {

    FileStream::FileStream(const FilePtr &f, uint64_t base) : file(f), baseOffset(base) // NOLINT
    {
    }

    const uint8_t *FileStream::Data(uint64_t offset)
    {
        return nullptr;
    }

    void FileStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        file->ReadData(baseOffset + offset, size, out);
    }

    const uint8_t *RawPtrStream::Data(uint64_t offset)
    {
        return data + offset;
    }

    void RawPtrStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        memcpy(out, data + offset, size);
    }
} // namespace sky::rhi