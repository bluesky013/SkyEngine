//
// Created by Zach Lee on 2023/9/12.
//

#pragma once

#include <rhi/Core.h>
#include <core/file/FileSystem.h>
#include <fstream>
#include <functional>

namespace sky::rhi {

    class FileStream : public IUploadStream {
    public:
        FileStream(const FilePtr &f, uint64_t base);
        ~FileStream() override = default;

        const uint8_t *Data(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        FilePtr file;
        uint64_t baseOffset;
    };

    class RawPtrStream : public IUploadStream {
    public:
        explicit RawPtrStream(const uint8_t *ptr) : data(ptr) {}
        ~RawPtrStream() override = default;

        const uint8_t *Data(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        const uint8_t *data;
    };

    template <typename T>
    class TRawBufferStream : public IUploadStream {
    public:
        explicit TRawBufferStream(std::vector<T> &&ptr) : data(std::move(ptr)) {}
        ~TRawBufferStream() override = default;

        const uint8_t *Data(uint64_t offset) override
        {
            return reinterpret_cast<const uint8_t*>(data.data());
        }

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override
        {
            memcpy(out, reinterpret_cast<const uint8_t*>(data.data()) + offset, size);
        }
    private:
        std::vector<T> data;
    };

    using RawBufferStream = TRawBufferStream<uint8_t>;

} // namespace sky::rhi