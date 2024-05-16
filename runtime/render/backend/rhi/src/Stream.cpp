//
// Created by Zach Lee on 2023/9/12.
//

#include <rhi/Stream.h>

namespace sky::rhi {

    FileStream::FileStream(const FilePtr &f, uint64_t base) : file(f), baseOffset(base)
    {
    }

    const uint8_t *FileStream::GetData(uint64_t offset)
    {
//        if (!hostData) {
//            auto fileSize = stream.tellg();
//            hostData.reset(new uint8_t[stream.tellg()]);
//            stream.seekg(static_cast<int>(baseOffset + offset), std::ios::beg);
//            stream.read((char *)hostData.get(), fileSize);
//            stream.close();
//        }
//
//        return hostData.get() + offset;
        return nullptr;
    }

    void FileStream::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        file->ReadData(baseOffset + offset, size, out);
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