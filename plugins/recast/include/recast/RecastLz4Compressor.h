//
// Created by blues on 2024/10/11.
//

#pragma once

#include <DetourTileCacheBuilder.h>
#include <cstdint>

namespace sky {
    class ICompressor;
} // namespace sky

namespace sky::ai {

    struct NaviCompressor : public dtTileCacheCompressor {
    public:
        NaviCompressor();
        ~NaviCompressor() override = default;

        int maxCompressedSize(const int bufferSize) override;

        dtStatus compress(const uint8_t* buffer, const int bufferSize, uint8_t* compressed,
                          const int maxCompressedSize, int* compressedSize) override;

        dtStatus decompress(const uint8_t* compressed, const int compressedSize,
                            uint8_t* buffer, const int maxBufferSize, int* bufferSize) override;

        ICompressor* compressor = nullptr;
    };

    NaviCompressor* GetOrCreateCompressor();

} // namespace sky::ai