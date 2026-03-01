//
// Created by blues on 2024/10/10.
//


#include <compression/LZ4.h>
#include <lz4.h>

namespace sky {

    uint32_t LZ4Compressor::CompressBound(uint32_t inDataSize)
    {
        return static_cast<uint32_t>(LZ4_compressBound(static_cast<int32_t>(inDataSize)));
    }

    CompressResult LZ4Compressor::Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &compressedData, uint32_t flags)
    {
        int outputSize = LZ4_compress_default(reinterpret_cast<const char*>(inData.data()),
            reinterpret_cast<char*>(compressedData.data()),
            static_cast<int32_t>(inData.size()),
            static_cast<int32_t>(compressedData.size()));

        if (outputSize < 0) {
            return {false, 0};
        }

        return {true, outputSize};
    }

    CompressResult LZ4Compressor::DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &decompressedData, uint32_t flags)
    {
        int outputSize = LZ4_decompress_safe(reinterpret_cast<const char*>(inData.data()),
            reinterpret_cast<char*>(decompressedData.data()),
            static_cast<int32_t>(inData.size()),
            static_cast<int32_t>(decompressedData.size()));

        if (outputSize < 0) {
            return {false, 0};
        }

        return {true, outputSize};
    }
} // namespace sky