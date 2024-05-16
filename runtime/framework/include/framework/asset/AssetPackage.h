//
// Created by blues on 2023/12/22.
//

#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <core/platform/Platform.h>
#include <core/util/Uuid.h>
#include <core/file/FileSystem.h>

namespace sky {

    struct SourceAssetInfo {
        std::string loadPath;
        uint32_t hash = 0;
    };

    using SourceAssetIDMap = std::unordered_map<Uuid, SourceAssetInfo>;

    class AssetPackage {
    public:
        AssetPackage() = default;
        ~AssetPackage() = default;

        void LoadFromFile(const FileSystemPtr &fs, const std::string &path);
        void SaveToFile(const FileSystemPtr &fs, const std::string &path);

        const Uuid &RegisterAsset(const Uuid &uuid, const SourceAssetInfo &info);
        void RemoveAsset(const Uuid &uuid);
        const SourceAssetInfo *GetAssetInfo(const Uuid &uuid) const;

        void Merge(const AssetPackage &package);

        const SourceAssetIDMap &GetIDMap() const { return assetLists; }
    private:
        mutable std::mutex mutex;
        SourceAssetIDMap assetLists;
    };

} // namespace sky