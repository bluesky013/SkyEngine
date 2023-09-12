//
// Created by Zach Lee on 2023/9/12.
//

#include <rhi/FImageStream.h>

namespace sky::rhi {

    FImageStream::FImageStream(const std::string &path) : stream(path, std::ios::binary)
    {
    }

    void FImageStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        if (!stream.is_open()) {
            return;
        }

        stream.seekg(static_cast<int>(offset), std::ios::beg);
        stream.read(reinterpret_cast<char *>(out), static_cast<int64_t>(size));
    }

    void RawImageStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        memcpy(out, data + offset, size);
    }
} // namespace sky::rhi