#include "Lz4Compressor.h"

#include <framework/compression/Compressor.h>

#include <lz4.h>

#include <limits>

namespace sky {
    namespace {
        class Lz4Compressor final : public ICompressor {
        public:
            uint32_t CompressBound(uint32_t inDataSize) override
            {
                if (inDataSize > LZ4_MAX_INPUT_SIZE)
                    return 0;
                const int result = LZ4_compressBound(static_cast<int>(inDataSize));
                return result > 0 ? static_cast<uint32_t>(result) : 0;
            }

            CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &output, uint32_t flags) override
            {
                if (flags != 0 || inData.size() > LZ4_MAX_INPUT_SIZE || (!inData.empty() && inData.data() == nullptr) || output.empty() ||
                    output.data() == nullptr || output.size() > std::numeric_limits<int>::max())
                    return {false, 0};
                const char empty = 0;
                const int  result =
                    LZ4_compress_default(inData.empty() ? &empty : reinterpret_cast<const char *>(inData.data()),
                                         reinterpret_cast<char *>(output.data()), static_cast<int>(inData.size()), static_cast<int>(output.size()));
                return result > 0 ? CompressResult{true, static_cast<uint32_t>(result)} : CompressResult{false, 0};
            }

            CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &output, uint32_t flags) override
            {
                if (flags != 0 || inData.empty() || inData.data() == nullptr || inData.size() > std::numeric_limits<int>::max() ||
                    (!output.empty() && output.data() == nullptr) || output.size() > std::numeric_limits<int>::max())
                    return {false, 0};
                char      empty  = 0;
                const int result = LZ4_decompress_safe(reinterpret_cast<const char *>(inData.data()),
                                                       output.empty() ? &empty : reinterpret_cast<char *>(output.data()),
                                                       static_cast<int>(inData.size()), static_cast<int>(output.size()));
                return result >= 0 ? CompressResult{true, static_cast<uint32_t>(result)} : CompressResult{false, 0};
            }
        };
    } // namespace

    std::shared_ptr<ICompressor> CreateBuiltinLz4Compressor()
    {
        return std::make_shared<Lz4Compressor>();
    }
} // namespace sky
