//
// Created by Zach on 2022/8/8.
//

#include <framework/asset/Asset.h>

#include <cstring>
#include <limits>

namespace sky {

    void AssetBase::AddBulk(const void *data, size_t size)
    {
        if (size != 0 && data != nullptr) {
            const auto *begin = reinterpret_cast<const uint8_t *>(data);
            bulk.insert(bulk.end(), begin, begin + size);
        }
    }

    std::vector<uint8_t> AssetBase::ReadBulk(uint64_t offset, uint64_t size) const
    {
        if (productStream == nullptr || size == 0 || size > SIZE_MAX || offset > UINT64_MAX - bulkStart ||
            bulkStart + offset > uint64_t(std::numeric_limits<std::streamoff>::max()) ||
            size > uint64_t(std::numeric_limits<std::streamsize>::max())) {
            return {};
        }
        std::vector<uint8_t> out(static_cast<size_t>(size));
        auto                &stream = productStream->GetStream();
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(bulkStart + offset), std::ios::beg);
        if (!stream || !stream.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(size)))
            return {};
        return out;
    }

    void AssetBase::AddDependencies(const Uuid &id)
    {
        dependencies.emplace_back(id);
    }

    void AssetBase::ResetDependencies()
    {
        dependencies.clear();
    }

    void AssetBase::BlockUntilLoaded() const
    {
        // Copy the handle under the lock, then wait OUTSIDE it (the cook completion resolves under the
        // same lock, so waiting while holding it would deadlock).
        std::shared_future<void> handle = GetWaitHandle();
        if (handle.valid()) {
            handle.wait();
        }
    }

} // namespace sky
