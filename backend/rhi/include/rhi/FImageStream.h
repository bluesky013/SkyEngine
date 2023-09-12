//
// Created by Zach Lee on 2023/9/12.
//

#pragma once

#include <rhi/Core.h>
#include <fstream>
#include <functional>

namespace sky::rhi {

    class FImageStream : public IImageStream {
    public:
        explicit FImageStream(const std::string &path);
        ~FImageStream() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        std::ifstream stream;
    };

    class RawImageStream : public IImageStream {
    public:
        explicit RawImageStream(uint8_t *ptr) : data(ptr) {}
        ~RawImageStream() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

    private:
        uint8_t *data;
    };

} // namespace sky::rhi