//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/AssetCommon.h>
#include <framework/asset/Asset.h>

#include <vector>
#include <unordered_map>
#include <mutex>

namespace sky {

    class AssetDataBase : public Singleton<AssetDataBase> {
    public:
        AssetDataBase() = default;
        ~AssetDataBase() override = default;

        void SetEngineFs(const NativeFileSystemPtr &fs);
        void SetWorkSpaceFs(const NativeFileSystemPtr &fs);

        const NativeFileSystemPtr &GetEngineFs() const { return engineFs; }
        const NativeFileSystemPtr &GetWorkSpaceFs() const { return workSpaceFs; }

        AssetSourcePtr RegisterAsset(const AssetSourcePath &path);
        AssetSourcePtr RegisterAsset(const std::string &path);
        AssetSourcePath QuerySource(const std::string &path);   // engine->workspace->custom

        AssetSourcePtr FindAsset(const Uuid &id);
        void RemoveAsset(const Uuid &id);

        FilePtr OpenFile(const AssetSourcePtr &src);

        void SetMarkedName(const Uuid& id, const std::string &name);

        void Load();
        void Save();
        void Reset();

        void Dump(std::ostream &stream);

        const std::unordered_map<Uuid, AssetSourcePtr> &GetSources() const { return idMap; }

    private:
        const NativeFileSystemPtr &GetFileSystemBySourcePath(const AssetSourcePath &path);

        NativeFileSystemPtr engineFs;
        NativeFileSystemPtr workSpaceFs;
        std::unordered_map<uint32_t, NativeFileSystemPtr> pluginFs;

        std::recursive_mutex assetMutex;
        std::unordered_map<AssetSourcePath, Uuid> pathMap;
        std::unordered_map<Uuid, AssetSourcePtr> idMap;
    };

} // namespace sky