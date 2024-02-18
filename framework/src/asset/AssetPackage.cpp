//
// Created by blues on 2023/12/22.
//

#include "framework/asset/AssetPackage.h"
#include <core/archive/StreamArchive.h>
#include <core/logger/Logger.h>

#include <fstream>

namespace sky {
    static const char* TAG = "Asset";

    void AssetPackage::LoadFromFile(const std::string &path)
    {
        std::fstream f(path, std::ios::in | std::ios::binary);
        if (!f.is_open()) {
            return;
        }

        IStreamArchive archive(f);

        uint32_t number = 0;
        archive.Load(number);
        for (uint32_t i = 0; i < number; ++i) {
            std::string id;
            archive.Load(id);

            SourceAssetInfo info = {};
            archive.Load(info.loadPath);
            archive.Load(info.hash);

            std::lock_guard<std::mutex> lock(mutex);
            assetLists.emplace(Uuid::CreateFromString(id), info);
        }
    }

    void AssetPackage::SaveToFile(const std::string &path)
    {
        std::fstream o(path, std::ios::out | std::ios::binary);
        OStreamArchive archive(o);

        std::lock_guard<std::mutex> lock(mutex);
        archive.Save(static_cast<uint32_t>(assetLists.size()));
        for (auto &[id, info] : assetLists) {
            archive.Save(id.ToString());
            archive.Save(info.loadPath);
            archive.Save(info.hash);
        }
    }

    const Uuid &AssetPackage::RegisterAsset(const Uuid &uuid, const SourceAssetInfo &info)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assetLists.emplace(uuid, info);
        if (!iter.second) {
            LOG_I(TAG, "duplicated asset %s", uuid.ToString().c_str());
        }
        return iter.first->first;
    }

    void AssetPackage::RemoveAsset(const Uuid &uuid)
    {
        std::lock_guard<std::mutex> lock(mutex);
        assetLists.erase(uuid);
    }

    const SourceAssetInfo *AssetPackage::GetAssetInfo(const Uuid &uuid) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assetLists.find(uuid);
        return iter != assetLists.end() ? &iter->second : nullptr;
    }

    void AssetPackage::Merge(const AssetPackage &package)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[id, relativePath] : package.assetLists) {
            assetLists[id] = relativePath;
        }
    }

} // namespace sky