//
// Created by Zach Lee on 2023/9/12.
//

#pragma once

#include <rhi/Core.h>
#include <fstream>
#include <functional>

namespace sky::rhi {

    class ImageStream : public IImageStream {
    public:
        explicit ImageStream(const std::string &path);
        ~ImageStream() override = default;

        const uint8_t *GetData(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        std::ifstream stream;
        std::unique_ptr<uint8_t> hostData;
    };

    class RawImageStream : public IImageStream {
    public:
        explicit RawImageStream(const uint8_t *ptr) : data(ptr) {}
        ~RawImageStream() override = default;

        const uint8_t *GetData(uint64_t offset) override;
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        const uint8_t *data;
    };

} // namespace sky::rhi