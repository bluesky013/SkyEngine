#pragma once

#include <memory>

namespace sky {
    class ICompressor;
    std::shared_ptr<ICompressor> CreateBuiltinLz4Compressor();
} // namespace sky
