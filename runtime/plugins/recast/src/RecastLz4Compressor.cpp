//
// Created by blues on 2024/10/11.
//

#include <recast/RecastLz4Compressor.h>
#include <framework/compression/Compressor.h>

namespace sky::ai {

    NaviCompressor::NaviCompressor()
    {
        compressor = CompressionManager::Get()->GetCompressor(CompressionMethod::LZ4);
    }

    int NaviCompressor::maxCompressedSize(const int bufferSize)
    {
        return compressor != nullptr ? static_cast<int>(compressor->CompressBound(bufferSize)) : 0;
    }

    dtStatus NaviCompressor::compress(const uint8_t* buffer, const int bufferSize, uint8_t* compressed,
        const int maxCompressedSize, int* compressedSize)
    {
        if (compressor == nullptr) {
            return DT_FAILURE;
        }

        auto res = compressor->Compress(std::span<const uint8_t>(buffer, bufferSize),
            std::span<uint8_t>(compressed, maxCompressedSize), 0);

        if (!res.first) {
            return DT_FAILURE;
        }
        *compressedSize = static_cast<int>(res.second);
        return DT_SUCCESS;
    }

    dtStatus NaviCompressor::decompress(const uint8_t* compressed, const int compressedSize,
        uint8_t* buffer, const int maxBufferSize, int* bufferSize)
    {
        if (compressor == nullptr) {
            return DT_FAILURE;
        }

        auto res = compressor->DeCompress(std::span<const uint8_t>(compressed, compressedSize),
            std::span<uint8_t>(buffer, maxBufferSize), 0);
        if (!res.first) {
            return DT_FAILURE;
        }
        *bufferSize = static_cast<int>(res.second);
        return DT_SUCCESS;
    }

} // namespace sky::ai