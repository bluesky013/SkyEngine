//
// Created by blues on 2024/10/9.
//

#pragma once

#include <core/environment/Singleton.h>
#include <span>

namespace sky {

    enum class CompressionMethod : uint32_t  {
        LZ4,
        ZLIB
    };

    using CompressResult = std::pair<bool, uint32_t>;

    class ICompressor {
    public:
        ICompressor() = default;
        virtual ~ICompressor() = default;

        virtual uint32_t CompressBound(uint32_t inDataSize) = 0;
        virtual CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &compressedData, uint32_t flags) = 0;
        virtual CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &decompressedData, uint32_t flags) = 0;
    };

    class CompressionManager : public Singleton<CompressionManager> {
    public:
        CompressionManager() = default;
        ~CompressionManager() override = default;

        ICompressor* GetCompressor(CompressionMethod method) const;

        void Register(CompressionMethod method, ICompressor* compressor);
        void UnRegister(CompressionMethod method);

    private:
        std::unordered_map<CompressionMethod, std::unique_ptr<ICompressor>> compressionMethods;
    };

} // namespace sky
