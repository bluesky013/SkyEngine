//
// Created by Zach Lee on 2023/9/12.
//

#include <rhi/Stream.h>

namespace sky::rhi {

    FileStream::FileStream(const std::string &path) : stream(path, std::ios::binary)
    {
    }

    const uint8_t *FileStream::GetData(uint64_t offset)
    {
        if (!stream.is_open()) {
            return nullptr;
        }

        if (!hostData) {
            auto fileSize = stream.tellg();
            hostData.reset(new uint8_t[stream.tellg()]);
            stream.seekg(0);
            stream.read((char *)hostData.get(), fileSize);
            stream.close();
        }

        return hostData.get() + offset;
    }

    void FileStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        if (!stream.is_open()) {
            return;
        }

        stream.seekg(static_cast<int>(offset), std::ios::beg);
        stream.read(reinterpret_cast<char *>(out), static_cast<int64_t>(size));
    }

    const uint8_t *RawPtrStream::GetData(uint64_t offset)
    {
        return data + offset;
    }

    void RawPtrStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        memcpy(out, data + offset, size);
    }
} // namespace sky::rhi