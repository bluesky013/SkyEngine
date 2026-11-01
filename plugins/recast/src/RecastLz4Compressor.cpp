//
// Created by blues on 2024/10/11.
//

#include <recast/RecastLz4Compressor.h>

#include <framework/compression/Compressor.h>

#include <limits>

namespace sky::ai {

    int NaviCompressor::maxCompressedSize(const int bufferSize)
    {
        const auto compressor = CompressionManager::Get()->AcquireCompressor(CompressionMethod::LZ4);
        if (compressor == nullptr || bufferSize < 0) {
            return 0;
        }
        const auto bound = compressor->CompressBound(static_cast<uint32_t>(bufferSize));
        return bound <= static_cast<uint32_t>(std::numeric_limits<int>::max()) ? static_cast<int>(bound) : 0;
    }

    dtStatus
    NaviCompressor::compress(const uint8_t *buffer, const int bufferSize, uint8_t *compressed, const int maxCompressedSize, int *compressedSize)
    {
        const auto compressor = CompressionManager::Get()->AcquireCompressor(CompressionMethod::LZ4);
        if (compressor == nullptr || bufferSize < 0 || maxCompressedSize < 0 || compressedSize == nullptr) {
            return DT_FAILURE;
        }

        const auto res = compressor->Compress(std::span<const uint8_t>(buffer, bufferSize), std::span<uint8_t>(compressed, maxCompressedSize), 0);

        if (!res.first || res.second > static_cast<uint32_t>(maxCompressedSize)) {
            return DT_FAILURE;
        }
        *compressedSize = static_cast<int>(res.second);
        return DT_SUCCESS;
    }

    dtStatus
    NaviCompressor::decompress(const uint8_t *compressed, const int compressedSize, uint8_t *buffer, const int maxBufferSize, int *bufferSize)
    {
        const auto compressor = CompressionManager::Get()->AcquireCompressor(CompressionMethod::LZ4);
        if (compressor == nullptr || compressedSize < 0 || maxBufferSize < 0 || bufferSize == nullptr) {
            return DT_FAILURE;
        }

        const auto res = compressor->DeCompress(std::span<const uint8_t>(compressed, compressedSize), std::span<uint8_t>(buffer, maxBufferSize), 0);
        if (!res.first || res.second > static_cast<uint32_t>(maxBufferSize)) {
            return DT_FAILURE;
        }
        *bufferSize = static_cast<int>(res.second);
        return DT_SUCCESS;
    }

} // namespace sky::ai
