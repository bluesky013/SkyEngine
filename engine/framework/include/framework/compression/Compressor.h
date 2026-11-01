//
// Created by blues on 2024/10/9.
//

#pragma once

#include <core/environment/Singleton.h>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

namespace sky {

    enum class CompressionMethod : uint32_t { LZ4, ZLIB };

    using CompressResult = std::pair<bool, uint32_t>;

    class ICompressor {
    public:
        ICompressor()          = default;
        virtual ~ICompressor() = default;

        virtual uint32_t       CompressBound(uint32_t inDataSize)                                                                             = 0;
        virtual CompressResult Compress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &compressedData, uint32_t flags)     = 0;
        virtual CompressResult DeCompress(const std::span<const uint8_t> &inData, const std::span<uint8_t> &decompressedData, uint32_t flags) = 0;
    };

    class CompressionManager : public Singleton<CompressionManager> {
    public:
        CompressionManager();
        ~CompressionManager() override = default;

        std::shared_ptr<ICompressor> AcquireCompressor(CompressionMethod method) const;
        // Borrowed compatibility lookup; operations retain an AcquireCompressor handle instead.
        ICompressor *GetCompressor(CompressionMethod method) const;

        // Host-owned virtual dispatch keeps control-block code resident; consumes candidates even on rejection.
        // Configure quiescently; DLL providers supply a code lease retained by acquired handles.
        virtual bool Register(CompressionMethod method, ICompressor *compressor, std::shared_ptr<const void> codeLease = {});
        void         UnRegister(CompressionMethod method);
        bool         SetEnabled(CompressionMethod method, bool enabled);

    private:
        struct MethodState {
            std::shared_ptr<ICompressor> builtin;
            std::shared_ptr<ICompressor> replacement;
            bool                         enabled = true;
        };
        mutable std::mutex         mutex;
        std::array<MethodState, 2> methods;
    };

} // namespace sky
