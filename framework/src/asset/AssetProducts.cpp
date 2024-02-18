//
// Created by blues on 2023/12/31.
//

#include <framework/asset/AssetProducts.h>
#include <fstream>
#include <core/archive/StreamArchive.h>

namespace sky {

    void AssetProducts::LoadFromFile(const std::string &path)
    {
        std::fstream f(path, std::ios::in | std::ios::binary);
        if (!f.is_open()) {
            return;
        }

        IStreamArchive archive(f);

        uint32_t number = 0;
        archive.Load(number);
        for (uint32_t i = 0; i < number; ++i) {
            Uuid id;
            ProductAssetInfo info = {};
            archive.Load(reinterpret_cast<char *>(&id), sizeof(Uuid));
            std::lock_guard<std::mutex> lock(mutex);
            assetLists.emplace(id, info);
        }
    }

    void AssetProducts::SaveToFile(const std::string &path)
    {
        std::fstream o(path, std::ios::out | std::ios::binary);
        OStreamArchive archive(o);

        std::lock_guard<std::mutex> lock(mutex);
        archive.Save(static_cast<uint32_t>(assetLists.size()));
        for (auto &[id, info] : assetLists) {
            archive.Save(reinterpret_cast<const char*>(&id), sizeof(Uuid));
        }
    }

    void AssetProducts::AddAsset(const Uuid &uuid, const ProductAssetInfo &info)
    {
        std::lock_guard<std::mutex> lock(mutex);
        assetLists[uuid] = info;
    }

    void AssetProducts::RemoveAsset(const Uuid &uuid)
    {
        std::lock_guard<std::mutex> lock(mutex);
        assetLists.erase(uuid);
    }

    void AssetProducts::RegisterDependencies(const Uuid &src, const std::vector<Uuid> &dst)
    {
        std::lock_guard<std::mutex> lock(mutex);
        dependencies[src] = dst;
    }

    const ProductAssetInfo *AssetProducts::GetProduct(const Uuid &uuid) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assetLists.find(uuid);
        return iter != assetLists.end() ? &iter->second : nullptr;
    }

} // namespace sky