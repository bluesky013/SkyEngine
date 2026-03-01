//
// Created by blues on 2024/10/10.
//

#include <framework/compression/Compressor.h>
#include <core/platform/Platform.h>
namespace sky {
    ICompressor* CompressionManager::GetCompressor(CompressionMethod method) const
    {
        auto iter = compressionMethods.find(method);
        return iter != compressionMethods.end() ? iter->second.get() : nullptr;
    }

    void CompressionManager::Register(CompressionMethod method, ICompressor* compressor)
    {
        auto iter = compressionMethods.emplace(method, compressor);
        SKY_ASSERT(iter.second);
    }

    void CompressionManager::UnRegister(CompressionMethod method)
    {
        compressionMethods.erase(method);
    }

} // namespace sky