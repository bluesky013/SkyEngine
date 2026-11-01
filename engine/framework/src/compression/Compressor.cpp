//
// Created by blues on 2024/10/10.
//

#include <framework/compression/Compressor.h>

#if defined(SKY_FRAMEWORK_BUILTIN_LZ4)
    #include "Lz4Compressor.h"
#endif

namespace sky {

    CompressionManager::CompressionManager()
    {
#if defined(SKY_FRAMEWORK_BUILTIN_LZ4)
        methods[static_cast<size_t>(CompressionMethod::LZ4)].builtin = CreateBuiltinLz4Compressor();
#endif
    }

    std::shared_ptr<ICompressor> CompressionManager::AcquireCompressor(CompressionMethod method) const
    {
        const auto      index = static_cast<size_t>(method);
        std::lock_guard lock(mutex);
        if (index >= methods.size() || !methods[index].enabled)
            return {};
        const auto &entry = methods[index];
        return entry.replacement ? entry.replacement : entry.builtin;
    }

    ICompressor *CompressionManager::GetCompressor(CompressionMethod method) const
    {
        return AcquireCompressor(method).get();
    }

    bool CompressionManager::Register(CompressionMethod method, ICompressor *compressor)
    {
        std::shared_ptr<ICompressor> handle(compressor);
        const auto                   index = static_cast<size_t>(method);
        if (!compressor || index >= methods.size())
            return false;
        std::lock_guard lock(mutex);
        if (methods[index].replacement)
            return false;
        methods[index].replacement = std::move(handle);
        return true;
    }

    void CompressionManager::UnRegister(CompressionMethod method)
    {
        const auto                   index = static_cast<size_t>(method);
        std::shared_ptr<ICompressor> removed;
        {
            std::lock_guard lock(mutex);
            if (index < methods.size())
                removed = std::move(methods[index].replacement);
        }
    }

    bool CompressionManager::SetEnabled(CompressionMethod method, bool enabled)
    {
        const auto      index = static_cast<size_t>(method);
        std::lock_guard lock(mutex);
        if (index >= methods.size())
            return false;
        methods[index].enabled = enabled;
        return true;
    }

} // namespace sky
