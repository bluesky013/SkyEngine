//
// Created by Zach Lee on 2023/9/12.
//

#pragma once

#include <rhi/Core.h>
#include <fstream>
#include <functional>

namespace sky::rhi {

    class FileStream : public IStream {
    public:
        explicit FileStream(const std::string &path);
        ~FileStream() override = default;

        const uint8_t *GetData(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        std::ifstream stream;
        std::unique_ptr<uint8_t> hostData;
    };

    class RawPtrStream : public IStream {
    public:
        explicit RawPtrStream(const uint8_t *ptr) : data(ptr) {}
        ~RawPtrStream() override = default;

        const uint8_t *GetData(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        const uint8_t *data;
    };

} // namespace sky::rhi