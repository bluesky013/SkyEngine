//
// Created by blues on 2024/10/10.
//

#pragma once

#include <framework/compression/Compressor.h>

namespace sky {

    class LZ4Compressor : public ICompressor {
    public:
        LZ4Compressor() = default;
        ~LZ4Compressor() override = default;

        uint32_t CompressBound(uint32_t inDataSize) override;
        CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &compressedData, uint32_t flags) override;
        CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &decompressedData, uint32_t flags) override;
    };

} // namespace sky